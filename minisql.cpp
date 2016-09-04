#include<bits/stdc++.h>
using namespace std;

// include the sql parser
#include "SQLParser.h"

// contains printing utilities
#include "sqlhelper.h"

#define BEGAN_TABLE 0
#define RUN_TABLE 1
#define ENDED_TABLE 2

#define AGGMAX "max"
#define AGGMIN "min"
#define AGGAVG "avg"
#define AGGSUM "sum"

int numTables=0;
map<string, vector<string> > tables;

vector<string> split(const string &s, char delim) {
    stringstream ss(s);
    string item;
    vector<string> tokens;
    while (getline(ss, item, delim)) {
        tokens.push_back(item);
    }
    return tokens;
}

void parseMetadata() {
    ifstream inFile("metadata.txt");
    string line,curTable;
    int flag=ENDED_TABLE;
    while(inFile>>line) {
        if(line=="<begin_table>") {
            flag=BEGAN_TABLE;
            numTables++;
        }
        else if(line=="<end_table>") {
            flag=ENDED_TABLE;
        }
        else if(flag==BEGAN_TABLE) {
            vector<string> attributes;
            string newTable=line;
            tables[newTable]=attributes;
            curTable=line;
            flag=RUN_TABLE;
        }
        else {
            string newAttr=line;
            tables[curTable].push_back(newAttr);
        }
    }
    inFile.close();
}

bool validateSyntax(hsql::SQLParserResult* result) {
    if(!result->isValid) {
        cout<<"Invalid SQL!\n";
        cout<<result->errorMsg<<endl;
        cout<<"Error line: "<<result->errorLine+1<<endl;
        cout<<"Error column: "<<result->errorColumn+1<<endl;
        return false;    
    }
    return true;
}

bool validateScope(hsql::SQLStatement* stmt, string query) {

    if(stmt->type()!=hsql::kStmtSelect) {
        cout<<"Sorry! Query \""<<query<<"\" is out of scope!\n";
        return false;
    }
    return true;
}

bool checkExistsSingle(hsql::TableRef* table) {
    if(table->type==hsql::kTableName and tables.find(table->name)==tables.end()) {
        cout<<"Table "<<table->name<<" not found!\n";
        return false;
    }
    return true;
}

bool checkExistsMultiple(hsql::TableRef* table) {
    if(table->type==hsql::kTableCrossProduct) {
        for (hsql::TableRef* tbl : *table->list) {
            if(tables.find(tbl->name)==tables.end()) {
                cout<<"Table "<<tbl->name<<" not found!\n";
                return false;;
            }
        }
    }
    return true;
}

bool executeQueryTypeA(hsql::TableRef* table, hsql::SelectStatement* selectStmt) {
    if(table->type==hsql::kTableName and 
            (*selectStmt->selectList).size()==1 and 
            (*selectStmt->selectList)[0]->type==hsql::kExprStar) {

        ifstream tableFile((string)table->name+".csv");
        int i=0;
        string line;
        for(i=0;i<tables[table->name].size()-1;i++)
            cout<<table->name<<"."<<tables[table->name][i]<<",";
        cout<<table->name<<"."<<tables[table->name][i]<<endl;

        while(!tableFile.eof()) {
            tableFile>>line;
            cout<<line<<endl;
        }
        return true;
    }
    return false;
}

bool executeQueryTypeB(hsql::TableRef* table, hsql::SelectStatement* selectStmt) {
    if(table->type==hsql::kTableName and
            (*selectStmt->selectList).size()==1 and
            (*selectStmt->selectList)[0]->type==hsql::kExprFunctionRef) {

        string attr=(*selectStmt->selectList)[0]->expr->name;
        vector<string>::iterator it;
        it=find(tables[table->name].begin(),tables[table->name].end(),attr);

        if(it==tables[table->name].end()) {
            cout<<"Column "<<attr<<" not found!\n";
            return true;
        }

        int attrNum=it-tables[table->name].begin();

        string line;

        ifstream tableFile((string)table->name+".csv");

        long long sum=0,numLines=0,mx=LLONG_MIN,mn=LLONG_MAX;

        string aggType=(*selectStmt->selectList)[0]->name;

        transform(aggType.begin(), aggType.end(), aggType.begin(),
                [](unsigned char c) { return std::tolower(c); });

        if(aggType!=AGGMAX and aggType!=AGGMIN and aggType!=AGGAVG and aggType!=AGGSUM) {
            cout<<"Aggregate Function "<<aggType<<" not found!\n";
            cout<<"Available aggregate functions are max, min, avg and sum\n";
            return true;
        }

        while(!tableFile.eof()) {
            tableFile>>line;
            numLines++;
            vector<string> items=split(line,',');

            mx=max(stoll(items[attrNum]),mx);
            mn=min(stoll(items[attrNum]),mn);
            sum+=stoll(items[attrNum]);
        }

        if(aggType==AGGMAX) cout<<mx<<endl;
        else if(aggType==AGGMIN) cout<<mn<<endl;
        else if(aggType==AGGAVG) cout<<(double)sum/numLines<<endl;
        else if(aggType==AGGSUM) cout<<sum<<endl;

        return true;
    }
    return false;

}

bool executeQueryTypeC1(hsql::TableRef* table, hsql::SelectStatement* selectStmt) {
    if(table->type==hsql::kTableName) {
        vector<hsql::Expr*> attributes=(*selectStmt->selectList);
        vector<int> cols;
        for(int i=0;i<attributes.size();i++) {
            vector<string>::iterator it;
            it=find(tables[table->name].begin(),tables[table->name].end(),attributes[i]->name);
            if(it==tables[table->name].end()) {
                cout<<"Column "<<attributes[i]->name<<" not found!\n";
                return true;
            }
            else
                cols.push_back(it-tables[table->name].begin());
        }

        ifstream tableFile((string)table->name+".csv");
        string line;

        while(!tableFile.eof()) {
            tableFile>>line;
            vector<string> items=split(line,',');
            int i;
            for(i=0;i<cols.size()-1;i++) {
                cout<<items[cols[i]]<<",";
            }    		
            cout<<items[cols[i]]<<endl;
        }
        return true;
    }
    return false;
}

/*bool executeQueryTypeC2(hsql::TableRef* table, hsql::SelectStatement* selectStmt) {
	if(table->type==hsql::kTableCrossProduct) {
		
	}

}
*/


int main(int argc, char *argv[]) {

    if(argc<=1) {
        cout<<"Usage: ./minisql \"SELECT * FROM test;\"\n";
        return -1;
    }
    const string input=argv[1];

    vector<string> queries=split(input, ';');

    for(string query : queries) {

        hsql::SQLParserResult* result=hsql::SQLParser::parseSQLString(query);

        if(!validateSyntax(result)) continue;

    	hsql::SQLStatement* stmt=result->getStatement(0);

        if(!validateScope(stmt, query)) continue;

        parseMetadata();

        hsql::SelectStatement* selectStmt=(hsql::SelectStatement*)stmt;

        hsql::TableRef* table=selectStmt->fromTable;

        if(!checkExistsSingle(table)) continue;      

        if(!checkExistsMultiple(table)) continue;

        if(executeQueryTypeA(table, selectStmt)) continue;

        if(executeQueryTypeB(table, selectStmt)) continue;

        if(executeQueryTypeC1(table, selectStmt)) continue;
        
        /*if(executeQueryTypeC2(table, selectStmt)) continue;*/

    }    
    return 0;
}

