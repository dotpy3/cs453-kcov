#include <cstdio>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <utility>

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
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

// A Rewriter helps us manage the code rewriting task.
Rewriter TheRewriter;

class Branch {
private:
    unsigned int lineNumber;
    string condition;
public:
    Branch(Stmt* stmt, SourceManager& srcmgr, Expr* cn) : condition(TheRewriter.ConvertToString(cn)) {
        lineNumber = srcmgr.getExpansionLineNumber(stmt->getLocStart());
    }

    string getCondition() { return condition; }
    unsigned int getLineNumber() { return lineNumber; }
};

class MyASTVisitor : public RecursiveASTVisitor<MyASTVisitor>
{
private:
	int nbStmt;
    ASTContext *context;
    vector<Branch> branches;
    bool firstFuncInstantation;
    SourceLocation funcInstantation;

    SourceManager& getSrcMngr() {
        return context->getSourceManager();
    }

public:
	MyASTVisitor(CompilerInstance* CI) {
		nbStmt = 0;
        context = &(CI->getASTContext());
        branches = vector<Branch>();
        firstFuncInstantation = false;
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

    void AddCompleteConditionCoverageEvaluation(unsigned int brNmr, Expr* condition) {
        // Adds a coverage evaluation for the brNmr-d branch at the location of the condition
        string appendBefore = "((";
        string appendAfter = " || !VisitedElseBranch(" + int_to_string<unsigned int>(brNmr)
            + ")) && VisitedThenBranch(" + int_to_string<unsigned int>(brNmr) + "))";
        TheRewriter.InsertTextAfter(condition->getLocStart(), appendBefore);
        TheRewriter.InsertTextBefore(condition->getLocEnd(), appendAfter);
    }

    unsigned int addBranch(Branch b) {
        // Adds a branch to the branches list, returns the index of the branch
        branches.push_back(b);
        return branches.size() - 1;
    }

    bool VisitStmt(Stmt *s) {
        // Fill out this function for your homework
        // llvm::outs() << "On visite un statement\n";
    	if (ForStmt *forS = dyn_cast<ForStmt>(s)) {
    		// For Statement
            AddCompleteConditionCoverageEvaluation(addBranch(Branch(forS, getSrcMngr(), forS->getCond())), forS->getCond());
    	} else if (IfStmt *ifS = dyn_cast<IfStmt>(s)) {
    		// For Statement
            AddCompleteConditionCoverageEvaluation(addBranch(Branch(ifS, getSrcMngr(), ifS->getCond())), ifS->getCond());
    	} else if (DoStmt *doS = dyn_cast<DoStmt>(s)) {
    		// Do Statement
            AddCompleteConditionCoverageEvaluation(addBranch(Branch(doS, getSrcMngr(), doS->getCond())), doS->getCond());
    	} else if (CaseStmt *swC = dyn_cast<CaseStmt>(s)) {
    		// Switch Case Statement
            //addBranch(new Branch(swC, getSrcMngr(), swC->getCond()));
            //nbBranches += 1;
    	} else if (ConditionalOperator *ternS = dyn_cast<ConditionalOperator>(s)) {
            // Conditional Operator
            AddCompleteConditionCoverageEvaluation(addBranch(Branch(ternS, getSrcMngr(), ternS->getCond())), ternS->getCond());
        } else if (SwitchStmt *swS = dyn_cast<SwitchStmt>(s)) {
    		// Switch Statement
    		if (hasImplicitDefault(swS)) {
    			//
    		}
    	} else if (WhileStmt *whS = dyn_cast<WhileStmt>(s)) {
    		// While statement
            AddCompleteConditionCoverageEvaluation(addBranch(Branch(whS, getSrcMngr(), whS->getCond())), whS->getCond());
    	} else if (DefaultStmt *defS = dyn_cast<DefaultStmt>(s)) {
    		// Default Statement
    	}
        return true;
    }
    
    bool VisitFunctionDecl(FunctionDecl *f) {
        if (!f->isExternC() && firstFuncInstantation == false) {
            firstFuncInstantation = true;
            funcInstantation = f->getSourceRange().getBegin();
        }
        if (f->isMain()) {
            TheRewriter.InsertTextBefore(f->getSourceRange().getEnd(), "\nWriteResult();\n");
        }
        return true;
    }

    bool WriteInitialization(string FileRadical) {
        string StructCreation = "struct BranchTest {\n";
        StructCreation += "\tunsigned int lineNumber;\n";
        StructCreation += "\tunsigned int thenBrExec;\n";
        StructCreation += "\tunsigned int elseBrExec;\n";
        StructCreation += "\tchar* condExpression;\n";
        StructCreation += "};\n";

        string BranchTestConstructor = "void InitBranch(BranchTest* br, unsigned int givenLineNumber, char* givenCondExpression) {\n";
        BranchTestConstructor += "\tbr->lineNumber = givenLineNumber;\n\tbr->condExpression = givenCondExpression;\n\tbr->thenBrExec = 0;\n";
        BranchTestConstructor += "\tbr->elseBrExec = 0;\n}\n";

        string BranchThenVisitAcceptance = "bool VisitedThenBranch(unsigned int brId) {\n";
        BranchThenVisitAcceptance += "\tall_branches[brId].thenBrExec++;\n";
        BranchThenVisitAcceptance += "\treturn true;\n";
        BranchThenVisitAcceptance += "}\n";

        string BranchElseVisitAcceptance = "bool VisitedElseBranch(unsigned int brId) {\n";
        BranchElseVisitAcceptance += "\tall_branches[brId].elseBrExec++;\n";
        BranchElseVisitAcceptance += "\treturn true;\n";
        BranchElseVisitAcceptance += "}\n";

        string BasicInclude = "#include <stdio.h>\n";

        string NumberOfBranches = "#define NB_BRANCHES " + int_to_string<int>(branches.size()) + "\n\nBranchTest all_branches[NB_BRANCHES];\n";

        string BranchesConditions = "char* BranchesConditions[NB_BRANCHES] = {";
        if (branches.size() > 0) {
            BranchesConditions += "\"";
            BranchesConditions += branches[0].getCondition();
            BranchesConditions += "\"";
        }

        string BranchesInit;
        for (unsigned int i = 0; i < branches.size(); i++) {
            if (i != 0) {
                BranchesConditions += ", \"";
                BranchesConditions += branches[i].getCondition();
                BranchesConditions += "\"";
            }
            BranchesInit += "InitBranch(&(all_branches["; BranchesInit += int_to_string<unsigned int>(i); BranchesInit += "]), ";
            BranchesInit += int_to_string<unsigned int>(branches[i].getLineNumber()); BranchesInit += ", ";
            BranchesInit += "BranchesConditions["; BranchesInit += int_to_string<unsigned int>(i) + "]);\n";
        }

        BranchesConditions += "};\n";

        string WriteResult = "void WriteResult() {\n\tFILE* output = fopen(\"" + FileRadical + "-cov-measure.txt\"";
        WriteResult += ", \"w+\");\n\tfprintf(output, \"Line\\tThen\\tElse\\tCondition\")\n\tfor (unsigned int j = 0; j < NB_BRANCHES; j++) {\n";
        WriteResult += "\t\tfprintf(output, (\"%d\\t\", all_branches[j].lineNumber));\n\t\tfprintf(output, (\"%d\\t\", all_branches[j].thenBrExec));\n";
        WriteResult += "\t\tfprintf(output, (\"%d\\t\", all_branches[j].elseBrExec));\n\t\tfprintf(output, all_branches[j].condExpression);\n";
        WriteResult += "\t\tfprintf(output, \"\\n\");\n\t}\n\tfclose(output);\n}\n";
    }
};

class MyASTConsumer : public ASTConsumer
{
public:
    MyASTConsumer(CompilerInstance* CI) : Visitor(CI)
    {
    }

    virtual bool HandleTopLevelDecl(DeclGroupRef DR) {
        for (DeclGroupRef::iterator b = DR.begin(), e = DR.end(); b != e; ++b) {
            // Travel each function declaration using MyASTVisitor
            Visitor.TraverseDecl(*b);
        }
        return true;
    }

    bool WriteInitialization(string FileRadical) {
        return Visitor.WriteInitialization(FileRadical);
    }

private:
    MyASTVisitor Visitor;
};


int main(int argc, char *argv[])
{
    if (argc != 2) {
        llvm::errs() << "Usage: kcov <filename>\n";
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


    TheRewriter.setSourceMgr(SourceMgr, TheCompInst.getLangOpts());

    string origFileName = argv[1];
    string radical = origFileName.substr(0, origFileName.find_last_of(".c"));
    string newCFileName = radical + "-cov.c";

    // Set the main file handled by the source manager to the input file.
    const FileEntry *FileIn = FileMgr.getFile(argv[1]);
    SourceMgr.createMainFileID(FileIn);
    
    // Inform Diagnostics that processing of a source file is beginning. 
    TheCompInst.getDiagnosticClient().BeginSourceFile(TheCompInst.getLangOpts(),&TheCompInst.getPreprocessor());
    
    // Create an AST consumer instance which is going to get called by ParseAST.
    MyASTConsumer TheConsumer(&TheCompInst);

    // Parse the file to AST, registering our consumer as the AST consumer.
    ParseAST(TheCompInst.getPreprocessor(), &TheConsumer, TheCompInst.getASTContext());

    // Add the first process
    TheConsumer.WriteInitialization(radical);

    const RewriteBuffer *RewriteBuf = TheRewriter.getRewriteBufferFor(SourceMgr.getMainFileID());
    ofstream output(newCFileName.c_str());
    output << string(RewriteBuf->begin(), RewriteBuf->end());
    output.close();

    return 0;
}
