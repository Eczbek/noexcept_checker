#include <clang/AST/ASTConsumer.h>
#include <clang/AST/ParentMap.h>
#include <clang/AST/ParentMapContext.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/AST/TypeBase.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendPluginRegistry.h>
#include <xte/preproc/lift.hpp>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#pragma GCC diagnostic ignored "-Wtrigraphs"

using namespace std::literals;

struct ThrowExpr {
	clang::Type const* type;
	clang::FunctionDecl const* fromFuncDecl = nullptr;

	friend bool operator==(ThrowExpr lhs, ThrowExpr rhs) {
		return lhs.type == rhs.type;
	}
};

template<>
struct std::hash<ThrowExpr> {
	std::size_t operator()(ThrowExpr throwExpr) const {
		return std::hash<clang::Type const*>()(throwExpr.type);
	}
};

clang::ASTContext* astContext;
std::unordered_map<clang::FunctionDecl const*, std::optional<std::unordered_set<clang::Type const*>>> funcThrowSpecs;
std::unordered_map<clang::FunctionDecl const*, std::unordered_set<ThrowExpr>> funcThrowExprs;

clang::Decl const* getDeclContextFromStmt(clang::Stmt const& stmt) {
	if (clang::DynTypedNode const* iter = astContext->getParents(stmt).begin(); iter != astContext->getParents(stmt).end()) {
		if (clang::Decl const* decl = iter->get<clang::Decl>()) {
			return decl;
		}
		if (clang::Stmt const* stmt = iter->get<clang::Stmt>()) {
			return getDeclContextFromStmt(*stmt);
		}
	}
	return nullptr;
};

clang::FunctionDecl const* getFunctionDeclFromStmt(clang::Stmt const& stmt) {
	if (clang::Decl const* decl = getDeclContextFromStmt(stmt)) {
		return static_cast<clang::FunctionDecl const*>(decl->getNonClosureContext());
	}
	return nullptr;
};

struct Visitor : clang::RecursiveASTVisitor<Visitor> {
	bool VisitFunctionDecl(clang::FunctionDecl* funcDecl) {
		if (!funcThrowSpecs.contains(funcDecl)) {
			switch (funcDecl->getExceptionSpecType()) {
				case clang::EST_NoexceptFalse:
					if (auto* noexExpr = funcDecl->getType()->getAs<clang::FunctionProtoType>()->getNoexceptExpr()) { // TODO: Maybe ASTMatchers could make this more beautiful. -Christoph
						if (auto* constExpr = llvm::dyn_cast<clang::ConstantExpr>(noexExpr)) {
							if (auto* constSubExpr = constExpr->getSubExpr()) {
								if (auto* impCastExpr = llvm::dyn_cast<clang::ImplicitCastExpr>(constSubExpr)) {
									if (auto* impCastSubExpr = impCastExpr->getSubExpr()) {
										if (auto* declRefExpr = llvm::dyn_cast<clang::DeclRefExpr>(impCastSubExpr)) {
											if (declRefExpr->getNameInfo().getName().getAsString() == "bTHROWS") { // TODO: Identify dummy template by custom attribute instead of magical name. -Christoph
												funcThrowSpecs[funcDecl].emplace();
												for (auto targ : declRefExpr->template_arguments()) {
													auto [iter, success] = funcThrowSpecs[funcDecl]->emplace(targ.getArgument().getAsType().getTypePtr());
													if (!success) {
														llvm::errs() << std::format("exception specifier of function '{}' contains duplicate entry '{}'\n", funcDecl->getNameAsString(), clang::QualType(*iter, 0).getAsString());
													}
												}
												break;
											}
										}
									}
								}
							}
						}
					}
					[[fallthrough]];
				case clang::EST_None:
				case clang::EST_MSAny:
					funcThrowSpecs[funcDecl] = std::nullopt;
					break;
				case clang::EST_DynamicNone:
				case clang::EST_NoThrow:
				case clang::EST_BasicNoexcept:
				case clang::EST_NoexceptTrue:
					funcThrowSpecs[funcDecl].emplace();
					break;
				case clang::EST_Dynamic:
					funcThrowSpecs[funcDecl].emplace();
					for (clang::QualType qualType : funcDecl->getType()->getAs<clang::FunctionProtoType>()->exceptions()) {
						auto [_, success] = funcThrowSpecs[funcDecl]->emplace(qualType.getTypePtr());
						if (!success) {
							llvm::errs() << std::format("exception specifier of function '{}' contains duplicate entry '{}'\n", funcDecl->getNameAsString(), qualType.getUnqualifiedType().getAsString());
						}
					}
					break;
				default:;
			}
		}
		return true;
	}

	bool VisitCallExpr(clang::CallExpr* callExpr) {
		if (clang::FunctionDecl* calledFuncDecl = callExpr->getDirectCallee()) {
			this->VisitFunctionDecl(calledFuncDecl);
			if (clang::FunctionDecl const* thisFuncDecl = getFunctionDeclFromStmt(*callExpr)) {
				if (auto const& throwSpec = funcThrowSpecs[calledFuncDecl]) {
					for (auto type : *throwSpec) {
						funcThrowExprs[thisFuncDecl].emplace(type, calledFuncDecl);
					}
				}
			}
		}
		return true;
	}

	bool VisitCXXConstructExpr(clang::CXXConstructExpr* constructExpr) {
		this->VisitFunctionDecl(constructExpr->getConstructor());
		return true;
	}

	bool VisitCXXThrowExpr(clang::CXXThrowExpr* throwExpr) {
		if (clang::Expr* thrownExpr = throwExpr->getSubExpr()) {
			if (clang::FunctionDecl const* funcDecl = getFunctionDeclFromStmt(*throwExpr)) {
				funcThrowExprs[funcDecl].emplace(thrownExpr->getType().getCanonicalType().getTypePtr());
			}
		}
		return true;
	}
};

struct Consumer : clang::ASTConsumer {
	void HandleTranslationUnit(clang::ASTContext& astContext) override {
		::astContext = &astContext;
		Visitor().TraverseDecl(astContext.getTranslationUnitDecl());
	}

	~Consumer() {
		for (auto [funcDecl, throwSpec] : funcThrowSpecs) {
			if (throwSpec) {
				for (auto type : *throwSpec) {
					if (!funcThrowExprs[funcDecl].contains({ type })) {
						llvm::errs() << std::format("exception specifier of function '{}' should not contain '{}'\n", funcDecl->getNameAsString(), clang::QualType(type, 0).getAsString());
					}
				}
			} else if (funcThrowExprs[funcDecl].size()) {
				for (auto [type, from] : funcThrowExprs[funcDecl]) {
					if (from) {
						llvm::errs() << std::format("exception specifier of function '{}' should contain '{}' (from uncaught call to function '{}')\n", funcDecl->getNameAsString(), clang::QualType(type, 0).getAsString(), from->getNameAsString());
					} else {
						llvm::errs() << std::format("exception specifier of function '{}' should contain '{}' (from uncaught throw expression)\n", funcDecl->getNameAsString(), clang::QualType(type, 0).getAsString());
					}
				}
			} else {
				llvm::errs() << std::format("exception specifier of function '{}' should be 'noexcept'\n", funcDecl->getNameAsString());
			}
		}
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
