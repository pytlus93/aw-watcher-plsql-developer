#pragma once
#include <string>
#include <vector>
#include <set>
#include <cctype>
namespace activity {
// Conservative lexical extraction, not a SQL validator. Literal/comment contents are discarded.
inline std::string sqlReferences(const std::string& sql) {
    std::vector<std::string> tokens;
    for(size_t i=0;i<sql.size()&&tokens.size()<8192;) {
        unsigned char c=sql[i];
        if(i+1<sql.size()&&sql[i]=='-'&&sql[i+1]=='-') {i=sql.find('\n',i+2);if(i==std::string::npos) break;continue;}
        if(i+1<sql.size()&&sql[i]=='/'&&sql[i+1]=='*') {i=sql.find("*/",i+2);if(i==std::string::npos) break;i+=2;continue;}
        if((c=='q'||c=='Q')&&i+2<sql.size()&&sql[i+1]=='\'') {
            char close=sql[i+2];if(close=='[')close=']';else if(close=='(')close=')';else if(close=='{')close='}';else if(close=='<')close='>';
            std::string terminator;terminator+=close;terminator+='\'';i=sql.find(terminator,i+3);if(i==std::string::npos)break;i+=2;continue;
        }
        if(c=='\'') {++i;while(i<sql.size()) {if(sql[i++]=='\'') {if(i<sql.size()&&sql[i]=='\''){++i;continue;}break;}}continue;}
        if(c=='"') {
            size_t begin=i++;bool closed=false;
            while(i<sql.size()) if(sql[i++]=='"') {if(i<sql.size()&&sql[i]=='"'){++i;continue;}closed=true;break;}
            if(closed&&i-begin<=260) tokens.push_back(sql.substr(begin,i-begin));continue;
        }
        if(std::isalpha(c)||c=='_'||c>=128) {
            size_t begin=i++;
            while(i<sql.size()&&(std::isalnum(static_cast<unsigned char>(sql[i]))||sql[i]=='_'||sql[i]=='$'||sql[i]=='#'||static_cast<unsigned char>(sql[i])>=128)) ++i;
            if(i-begin<=128) {auto token=sql.substr(begin,i-begin);for(char& v:token) if(static_cast<unsigned char>(v)<128)v=static_cast<char>(std::toupper(static_cast<unsigned char>(v)));tokens.push_back(token);}
        } else {if(c=='.'||c=='('||c==')'||c==','||c==';'||c=='@') tokens.push_back(std::string(1,static_cast<char>(c)));++i;}
    }
    auto identifier=[](const std::string& t){return !t.empty()&&(t[0]=='"'||std::isalpha(static_cast<unsigned char>(t[0]))||t[0]=='_'||static_cast<unsigned char>(t[0])>=128);};
    std::set<std::string> aliases;
    for(size_t i=0;i+2<tokens.size();i++) if(identifier(tokens[i])&&tokens[i+1]=="AS"&&tokens[i+2]=="(") aliases.insert(tokens[i]);
    std::set<std::string> found;
    for(size_t i=0;i+1<tokens.size()&&found.size()<16;i++) {
        if(tokens[i]!="FROM"&&tokens[i]!="JOIN"&&tokens[i]!="UPDATE"&&tokens[i]!="INTO")continue;
        size_t j=i+1;if(!identifier(tokens[j])||tokens[j]=="SELECT"||tokens[j]=="TABLE")continue;
        std::string name=tokens[j++];
        while(j+1<tokens.size()&&(tokens[j]=="."||tokens[j]=="@")&&identifier(tokens[j+1])) {name+=tokens[j]+tokens[j+1];j+=2;}
        if(!aliases.count(name)) found.insert(name);
    }
    std::string result;for(auto& name:found) {if(!result.empty())result+=", ";result+=name;}return result;
}
}
