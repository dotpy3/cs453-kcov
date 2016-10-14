#include <cstdio>
#include <string>
#include <iostream>
#include <sstream>
#include <map>
#include <utility>

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Basic/TargetOptions.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Parse/ParseAST.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Rewrite/Frontend/Rewriters.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/IntrusiveRefCntPtr.h"
#include "clang/Lex/HeaderSearch.h"
#include "clang/Frontend/Utils.h"

using namespace clang;
using namespace std;

template<typename T> string int_to_string(T i) {
	ostringstream convert;
	convert << i;
	return convert.str();
}

class MyASTVisitor : public RecursiveASTVisitor<MyASTVisitor>
{
private:
	int nbStmt;

public:
	MyASTVisitor() {
		nbStmt = 0;
	}

	bool PrintStmtDescription(string stmtType) {
		llvm::outs() << "\t" << stmtType << "\tID: " << int_to_string<int>(nbStmt++) << "\n";
	}

	bool hasImplicitDefault(SwitchStmt *s) {
        SwitchCase* cases = s->getSwitchCaseList();
        SwitchCase* testCase = cases;
        while (testCase) {
            if (DefaultStmt *defS = dyn_cast<DefaultStmt>(testCase)) {
                return false;
            } else {
                testCase = testCase->getNextSwitchCase();
            }
        }
        return true;
	}

    bool VisitStmt(Stmt *s) {
        // Fill out this function for your homework
        // llvm::outs() << "On visite un statement\n";
    	if (ForStmt *forS = dyn_cast<ForStmt>(s)) {
    		// For Statement
    		PrintStmtDescription("For");
    	} else if (IfStmt *ifS = dyn_cast<IfStmt>(s)) {
    		// For Statement
    		PrintStmtDescription("If");
    	} else if (DoStmt *doS = dyn_cast<DoStmt>(s)) {
    		// Do Statement
    		PrintStmtDescription("Do");
    	} else if (CaseStmt *swC = dyn_cast<CaseStmt>(s)) {
    		// Switch Case Statement
    		PrintStmtDescription("Case");
    	} else if (ConditionalOperator *ternS = dyn_cast<ConditionalOperator>(s)) {
            // Switch Case Statement
            PrintStmtDescription("?:");
        } else if (SwitchStmt *swS = dyn_cast<SwitchStmt>(s)) {
    		// Switch Statement
    		if (hasImplicitDefault(swS)) {
    			PrintStmtDescription("ImpDef.");
    		}
    	} else if (WhileStmt *whS = dyn_cast<WhileStmt>(s)) {
    		// Switch Statement
    		PrintStmtDescription("While");
    	} else if (DefaultStmt *defS = dyn_cast<DefaultStmt>(s)) {
    		// Switch Statement
    		PrintStmtDescription("Default");
    	}
        return true;
    }
    
    bool VisitFunctionDecl(FunctionDecl *f) {
        if (f->hasBody()) {
	        string name = f->getNameInfo().getName().getAsString();
	        llvm::outs() << "function: " << name << "\n";
        }
        return true;
    }
};

class MyASTConsumer : public ASTConsumer
{
public:
    MyASTConsumer() : Visitor() 
    {
    }

    virtual bool HandleTopLevelDecl(DeclGroupRef DR) {
        for (DeclGroupRef::iterator b = DR.begin(), e = DR.end(); b != e; ++b) {
            // Travel each function declaration using MyASTVisitor
            Visitor.TraverseDecl(*b);
        }
        return true;
    }

private:
    MyASTVisitor Visitor;
};


int main(int argc, char *argv[])
{
    if (argc != 2) {
        llvm::errs() << "Usage: kcov-branch-identify <filename>\n";
        return 1;
    }

	// CompilerInstance will hold the instance of the Clang compiler for us,
	// managing the various objects needed to run the compiler.
	CompilerInstance TheCompInst;
    
    // Diagnostics manage problems and issues in compile 
    TheCompInst.createDiagnostics(NULL, false);

    // Set target platform options 
    // Initialize target info with the default triple for our platform.
    TargetOptions *TO = new TargetOptions();
    TO->Triple = llvm::sys::getDefaultTargetTriple();
    TargetInfo *TI = TargetInfo::CreateTargetInfo(TheCompInst.getDiagnostics(), TO);
    TheCompInst.setTarget(TI);

    // FileManager supports for file system lookup, file system caching, and directory search management.
    TheCompInst.createFileManager();
    FileManager &FileMgr = TheCompInst.getFileManager();
    
    // SourceManager handles loading and caching of source files into memory.
    TheCompInst.createSourceManager(FileMgr);
    SourceManager &SourceMgr = TheCompInst.getSourceManager();
    
    // Prreprocessor runs within a single source file
    TheCompInst.createPreprocessor();
    
    // ASTContext holds long-lived AST nodes (such as types and decls) .
    TheCompInst.createASTContext();

    // Enable HeaderSearch option
    llvm::IntrusiveRefCntPtr<clang::HeaderSearchOptions> hso( new HeaderSearchOptions());
    HeaderSearch headerSearch(hso,
                              TheCompInst.getFileManager(),
                              TheCompInst.getDiagnostics(),
                              TheCompInst.getLangOpts(),
                              TI);

    // <Warning!!> -- Platform Specific Code lives here
    // This depends on A) that you're running linux and
    // B) that you have the same GCC LIBs installed that I do. 
    /*
    $ gcc -xc -E -v -
    ..
     /usr/local/include
     /usr/lib/gcc/x86_64-linux-gnu/4.4.5/include
     /usr/lib/gcc/x86_64-linux-gnu/4.4.5/include-fixed
     /usr/include
    End of search list.
    */
    const char *include_paths[] = {"/usr/local/include",
                "/usr/lib/gcc/x86_64-linux-gnu/4.4.6/include",
                "/usr/lib/gcc/x86_64-linux-gnu/4.4.6/include-fixed",
                "/usr/include"};

    for (int i=0; i<4; i++) 
        hso->AddPath(include_paths[i], 
                    clang::frontend::Angled, 
                    false, 
                    false);
    // </Warning!!> -- End of Platform Specific Code

    InitializePreprocessor(TheCompInst.getPreprocessor(), 
                  TheCompInst.getPreprocessorOpts(),
                  *hso,
                  TheCompInst.getFrontendOpts());


    // A Rewriter helps us manage the code rewriting task.
    Rewriter TheRewriter;
    TheRewriter.setSourceMgr(SourceMgr, TheCompInst.getLangOpts());

    // Set the main file handled by the source manager to the input file.
    const FileEntry *FileIn = FileMgr.getFile(argv[1]);
    SourceMgr.createMainFileID(FileIn);
    
    // Inform Diagnostics that processing of a source file is beginning. 
    TheCompInst.getDiagnosticClient().BeginSourceFile(TheCompInst.getLangOpts(),&TheCompInst.getPreprocessor());
    
    // Create an AST consumer instance which is going to get called by ParseAST.
    MyASTConsumer TheConsumer;

    // Parse the file to AST, registering our consumer as the AST consumer.
    ParseAST(TheCompInst.getPreprocessor(), &TheConsumer, TheCompInst.getASTContext());

    return 0;
}
