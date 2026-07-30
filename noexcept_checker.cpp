#include <clang/AST/ASTConsumer.h>
#include <clang/AST/ASTContext.h>
#include <clang/AST/CXXInheritance.h>
#include <clang/AST/Decl.h>
#include <clang/AST/ExprCXX.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/AST/StmtCXX.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendPluginRegistry.h>
#include <clang/Sema/Sema.h>
#include <clang/Sema/SemaConsumer.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/raw_ostream.h>
// TODO: Check for missing includes

#include <algorithm>
#include <format>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

using namespace std::literals;

clang::Sema* sema = nullptr;
clang::SourceManager* sourceManager = nullptr;
std::vector<void const*> scopeStack;
std::optional<std::unordered_set<clang::Type const*>> activeExceptions;
std::vector<clang::LambdaExpr const*> lambdaExprStack;
std::unordered_map<void const*, std::unordered_set<clang::Type const*>> thrownTypes;

struct Visitor : clang::RecursiveASTVisitor<Visitor> {
	bool TraverseFunctionDecl(clang::FunctionDecl* funcDecl) {
		if (!thrownTypes.contains(funcDecl)) {
			scopeStack.push_back(funcDecl);
			RecursiveASTVisitor::TraverseFunctionDecl(funcDecl);
			if (scopeStack.size() > 1) {
				thrownTypes[scopeStack[scopeStack.size() - 2]].insert_range(thrownTypes[funcDecl]);
			}
			scopeStack.pop_back();

			std::vector<clang::Type const*> specifiedTypes;
			switch (funcDecl->getExceptionSpecType()) {
				case clang::EST_Dynamic:
					for (clang::QualType qualType : funcDecl->getType()->getAs<clang::FunctionProtoType>()->exceptions()) {
						specifiedTypes.push_back(qualType.getTypePtr());
					}
					break;
				case clang::EST_NoexceptFalse:
					if (auto* noexExpr = funcDecl->getType()->getAs<clang::FunctionProtoType>()->getNoexceptExpr()) { // TODO: Maybe ASTMatchers could make this more beautiful. -Christoph
						if (auto* constExpr = llvm::dyn_cast<clang::ConstantExpr>(noexExpr)) {
							if (auto* constSubExpr = constExpr->getSubExpr()) {
								if (auto* impCastExpr = llvm::dyn_cast<clang::ImplicitCastExpr>(constSubExpr)) {
									if (auto* impCastSubExpr = impCastExpr->getSubExpr()) {
										if (auto* declRefExpr = llvm::dyn_cast<clang::DeclRefExpr>(impCastSubExpr)) {
											if (declRefExpr->getNameInfo().getName().getAsString() == "bTHROWS") { // TODO: Identify dummy template by custom attribute instead of magical name. -Christoph
												for (auto targ : declRefExpr->template_arguments()) {
													specifiedTypes.push_back(targ.getArgument().getAsType().getTypePtr());
												}
												break;
											}
										}
									}
								}
							}
						}
						if (!llvm::dyn_cast<clang::CXXBoolLiteralExpr>(noexExpr->IgnoreParenImpCasts())) {
							return true;
						}
					}
					[[fallthrough]];
				case clang::EST_None:
				case clang::EST_MSAny:
					if (thrownTypes[funcDecl].size()) {
						llvm::errs() << std::format("{}: exception specifier should explicitly list uncaught type(s): {:s}\n", funcDecl->getBeginLoc().printToString(*sourceManager), thrownTypes[funcDecl] | std::views::transform([](clang::Type const* type) { return std::format("'{}'", clang::QualType(type, 0).getAsString()); }) | std::views::join_with(", "sv));
					} else {
						llvm::errs() << std::format("{}: exception specifier should be noexcept\n", sourceManager->getExpansionLoc(funcDecl->getBeginLoc()).printToString(*sourceManager));
					}
					return true;
				default:;
			}
			std::unordered_set<clang::Type const*> propagatedTypes;
			for (clang::Type const* specifiedType : specifiedTypes) {
				auto [iter, success] = propagatedTypes.emplace(specifiedType);
				if (!success) {
					llvm::errs() << std::format("{}: exception specifier lists duplicate type: '{}'\n", sourceManager->getExpansionLoc(funcDecl->getBeginLoc()).printToString(*sourceManager), clang::QualType(*iter, 0).getAsString());
				}
			}
			for (clang::Type const* thrownType : thrownTypes[funcDecl]) {
				if (propagatedTypes.contains(thrownType)) {
					propagatedTypes.erase(thrownType);
				} else {
					llvm::errs() << std::format("{}: exception specifier omits uncaught type: '{}'\n", sourceManager->getExpansionLoc(funcDecl->getBeginLoc()).printToString(*sourceManager), clang::QualType(thrownType, 0).getAsString());
				}
			}
			for (clang::Type const* propagatedType : propagatedTypes) {
				llvm::errs() << std::format("{}: exception specifier lists caught or unthrown type: '{}'\n", sourceManager->getExpansionLoc(funcDecl->getBeginLoc()).printToString(*sourceManager), clang::QualType(propagatedType, 0).getAsString());
			}
		}
		return true;
	}

	bool TraverseLambdaExpr(clang::LambdaExpr* lambdaExpr) {
		TraverseFunctionDecl(lambdaExpr->getCallOperator());
		return true;
	}

	bool TraverseCXXConstructorDecl(clang::CXXConstructorDecl* constructorDecl) {
		TraverseFunctionDecl(constructorDecl);
		return true;
	}

	bool TraverseCXXMethodDecl(clang::CXXMethodDecl* methodDecl) {
		TraverseFunctionDecl(methodDecl);
		return true;
	}

	bool TraverseCXXTryStmt(clang::CXXTryStmt* tryStmt) {
		scopeStack.push_back(tryStmt);
		RecursiveASTVisitor::TraverseCXXTryStmt(tryStmt);
		scopeStack.pop_back();
		thrownTypes[scopeStack.back()].insert_range(std::move(thrownTypes[tryStmt]));
		return true;
	}

	bool TraverseCXXConstructExpr(clang::CXXConstructExpr* constructExpr) {
		TraverseFunctionDecl(constructExpr->getConstructor());
		thrownTypes[scopeStack.back()].insert_range(thrownTypes[constructExpr->getConstructor()]);
		// RecursiveASTVisitor::TraverseCXXConstructExpr(constructExpr);
		return true;
	}

	bool TraverseCXXTemporaryObjectExpr(clang::CXXTemporaryObjectExpr* tempObjExpr) {
		TraverseCXXConstructExpr(tempObjExpr);
		return true;
	}

	bool TraverseCallExpr(clang::CallExpr* callExpr) {
		if (clang::FunctionDecl* calledFuncDecl = callExpr->getDirectCallee()) {
			TraverseFunctionDecl(calledFuncDecl);
			thrownTypes[scopeStack.back()].insert_range(thrownTypes[calledFuncDecl]);
		}
		// RecursiveASTVisitor::TraverseCallExpr(callExpr);
		return true;
	}

	bool TraverseCXXOperatorCallExpr(clang::CXXOperatorCallExpr* operatorCallExpr) {
		TraverseCallExpr(operatorCallExpr);
		// RecursiveASTVisitor::TraverseCXXOperatorCallExpr(operatorCallExpr);
		return true;
	}

	bool TraverseCXXCatchStmt(clang::CXXCatchStmt* catchStmt) {
		clang::QualType caughtQualType = catchStmt->getCaughtType();
		if (caughtQualType.isNull()) {
			auto thrownCount = thrownTypes[scopeStack.back()].size();
			if (thrownCount) {
				llvm::errs() << std::format("{}: ellipsis catches {} type(s): {:s}\n", catchStmt->getBeginLoc().printToString(*sourceManager), thrownCount, thrownTypes[scopeStack.back()] | std::views::transform([](clang::Type const* type) { return std::format("'{}'", clang::QualType(type, 0).getAsString()); }) | std::views::join_with(", "sv));
			} else {
				llvm::errs() << std::format("{}: ellipsis catches no types\n", catchStmt->getBeginLoc().printToString(*sourceManager));
			}
			activeExceptions.emplace(std::move(thrownTypes[scopeStack.back()]));
			thrownTypes[scopeStack.back()].clear();
		} else {
			activeExceptions.emplace();
			clang::Type const* caughtType = caughtQualType.getNonReferenceType().getTypePtr();
			if (!std::erase_if(thrownTypes[scopeStack.back()], [&](clang::Type const* thrownType) -> bool {
				if (thrownType == caughtType) {
					activeExceptions->emplace(thrownType);
					if (!caughtQualType->isReferenceType()) {
						llvm::errs() << std::format("{}: caught type should be const reference: '{}'\n", catchStmt->getExceptionDecl()->getBeginLoc().printToString(*sourceManager), caughtQualType.getAsString());
					}
					return true;
				}
				if (caughtQualType->isReferenceType()) {
					auto paths = clang::CXXBasePaths(true, true, false);
					if (sema->IsDerivedFrom(catchStmt->getBeginLoc(), clang::QualType(thrownType, 0), caughtQualType, paths)) {
						for (clang::CXXBasePath const& path : paths) {
							if (sema->CheckBaseClassAccess(catchStmt->getBeginLoc(), caughtQualType, clang::QualType(thrownType, 0), path, 0) == clang::Sema::AR_accessible) {
								activeExceptions->emplace(thrownType);
								return true;
							}
						}
					}
				}
				return false;
			})) {
				llvm::errs() << std::format("{}: caught type was not thrown: '{}'\n", catchStmt->getExceptionDecl()->getBeginLoc().printToString(*sourceManager), clang::QualType(caughtType, 0).getAsString());
			}
		}
		RecursiveASTVisitor::TraverseCXXCatchStmt(catchStmt);
		activeExceptions.reset();
		return true;
	}

	bool TraverseCXXThrowExpr(clang::CXXThrowExpr* throwExpr) {
		RecursiveASTVisitor::TraverseCXXThrowExpr(throwExpr);
		if (clang::Expr* thrownExpr = throwExpr->getSubExpr()) {
			if (scopeStack.size()) {
				thrownTypes[scopeStack.back()].emplace(thrownExpr->getType().getTypePtr());
			} else {
				llvm::errs() << std::format("{}: throw in namespace scope cannot be caught\n", throwExpr->getBeginLoc().printToString(*sourceManager));
			}
		} else if (activeExceptions) {
			thrownTypes[scopeStack.back()] = std::move(*activeExceptions);
			activeExceptions.reset();
		} else {
			llvm::errs() << std::format("{}: rethrow without active exception will terminate\n", throwExpr->getBeginLoc().printToString(*sourceManager));
		}
		return true;
	}
};

struct Consumer : clang::SemaConsumer {
	void InitializeSema(clang::Sema& sema) override {
		::sema = &sema;
	}

	void HandleTranslationUnit(clang::ASTContext& astContext) override {
		sourceManager = &astContext.getSourceManager();
		Visitor().TraverseDecl(astContext.getTranslationUnitDecl());
	}
};

struct Action : clang::PluginASTAction {
protected:
	std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance&, llvm::StringRef) override {
		return std::make_unique<Consumer>();
	}

	bool ParseArgs(clang::CompilerInstance const&, std::vector<std::string> const&) override {
		return true;
	}
};

static clang::FrontendPluginRegistry::Add<Action> _("noexcept_checker", "Check noexcept specifiers");
