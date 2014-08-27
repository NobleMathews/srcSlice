/**
 * @file srcSliceHandler.hpp
 *
 * @copyright Copyright (C) 2013-2014 SDML (www.srcML.org)
 *
 * The srcML Toolkit is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The srcML Toolkit is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with the srcML Toolkit; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef INCLUDED_SRCSLICEHANDLER_HPP
#define INCLUDED_SRCSLICEHANDLER_HPP

#include <srcSAXHandler.hpp>
#include <SliceProfile.hpp>

#include <stdlib.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include <sstream>
std::vector<std::string> SplitLhsRhs(const std::string& str){
    std::vector<std::string> expr;
    expr.push_back(std::string());
    for(int i = 0; i<str.size(); ++i){
        if(str[i] == '='){
            expr.push_back(std::string());
            expr[1].append(str.substr(i+1, str.size()-1));
            break;
        }else{
            expr[0]+=str[i];
        }
    }
    return expr;
}

/* Split function for splitting strings by tokens. Works on sets of tokens or just one token*/
std::vector<std::string> Split(const std::string& str, const char* tok){
    std::size_t tokPos = str.find_first_of(tok);
    std::vector<std::string> result;
    std::size_t nextPos = 0, curPos = 0;
    while(curPos != std::string::npos){
        result.push_back(str.substr(nextPos, tokPos - nextPos));
        nextPos = tokPos+1;
        curPos = tokPos;
        tokPos = str.find_first_of(tok, nextPos);
    }
    return result;
}

class srcSliceHandler : public srcSAXHandler {

private :
    /*ParserState is a set of enums corresponding to srcML tags. Primarily, they're for addressing into the 
     *triggerField vector and figuring out which tags have been seen. It keeps a count of how many of each
     *tag is currently open. Increments at a start tag and decrements at an end tag*/
    enum ParserState {decl, expr, param, decl_stmt, expr_stmt, parameter_list, argument_list, call, ctrlflow, endflow, name, function, argument, index, block, type, init, op, literal, modifier, member_list, classn, nonterminal, empty, MAXENUMVALUE = empty};

    unsigned int fileNumber;
    unsigned int numArgs;
    
    int constructorNum;
    
    /*Hashing function/file names. This will accomplish that.*/
    std::hash<std::string> functionAndFileNameHash;

    /*Holds data for functions as we parse. Useful for going back to figuring out which function we're in*/
    FunctionData functionTmplt;

    /*keeps track of which functioni has been called. Useful for when argument slice profiles need to be updated*/
    std::string nameOfCurrentClldFcn;

    /*This is a pair of the current name and line number. Basically, this along with triggerfield
     *make up the meat of this slicer. Check the triggerfield for context (E.g., triggerField[init])
     *and then once you know the right tags are open, check currentNameAndLine to see what the name is
     *at that position and its line number to be stored in the slice profile*/
    NameLineNumberPair currentNameAndLine;
    std::vector<unsigned short int> triggerField;
    
    /*These two iterators keep track of where we are inside of the system dictionary. They're primarily so that
     *there's no need to do any nasty map.finds on the dictionary (since it's a nested map of maps). These must
     *be updated as the file is parsed*/
    FileFunctionVarMap::iterator FileIt;
    FunctionVarMap::iterator FunctionIt;
    VarMap::iterator varIt;

    /*Their names basically say what they do. Nested calls are an issue so the first one is a flag to figure out
     *if I need to do special processing on a name in a call. Might be a better way to do it. Investigate later. 
     *The second bool checks to see if the function that was just seen is a constructor. Do this because
     *functions and constructors are treated basically the same (and I mark them as the same thing in triggerfield)
     *but constructors need to be counted so a number can be appended to them to differentiate*/
    bool isNestedNameInCall;
    bool isConstructor;
public :
    SystemDictionary sysDict;
    srcSliceHandler(){
        fileNumber = 0;
        numArgs = 0;
        constructorNum = 0;

        isNestedNameInCall = false;
        isConstructor = false;

        nameOfCurrentClldFcn = "global";

        triggerField = std::vector<unsigned short int>(MAXENUMVALUE, 0);
    }
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"


    /*
    virtual void startDocument() {}
    virtual void endDocument() {}
    */

    /**
     * startRoot
     * @param localname the name of the element tag
     * @param prefix the tag prefix
     * @param URI the namespace of tag
     * @param nb_namespaces number of namespaces definitions
     * @param namespaces the defined namespaces
     * @param nb_attributes the number of attributes on the tag
     * @param nb_defaulted the number of defaulted attributes
     * @param attributes list of attribute name value pairs (localname/prefix/URI/value/end)
     *
     * SAX handler function for start of the root element.
     * Counts the root unit (if an archive, to avoid double count with startUnit).
     * Overide for desired behaviour.
     */
    virtual void startRoot(const char * localname, const char * prefix, const char * URI,
                           int num_namespaces, const struct srcsax_namespace * namespaces, int num_attributes,
                           const struct srcsax_attribute * attributes) {
        
        //push_element(localname, prefix);
    }
    /**
     * startUnit
     * @param localname the name of the element tag
     * @param prefix the tag prefix
     * @param URI the namespace of tag
     * @param nb_namespaces number of namespaces definitions
     * @param namespaces the defined namespaces
     * @param nb_attributes the number of attributes on the tag
     * @param nb_defaulted the number of defaulted attributes
     * @param attributes list of attribute name value pairs (localname/prefix/URI/value/end)
     *
     * SAX handler function for start of an unit.
     * Counts each unit tag (= filecount non-archive, = filecount + 1 if archive).
     * Overide for desired behaviour.
     */
    virtual void startUnit(const char * localname, const char * prefix, const char * URI,
                           int num_namespaces, const struct srcsax_namespace * namespaces, int num_attributes,
                           const struct srcsax_attribute * attributes) {
        fileNumber = functionAndFileNameHash(attributes[1].value);
        sysDict.fileTable.insert(std::make_pair(fileNumber, attributes[1].value));
        FileIt = sysDict.dictionary.insert(std::make_pair(fileNumber, FunctionVarMap())).first; //insert and keep track of most recent.         
        FunctionIt = FileIt->second.insert(std::make_pair(functionAndFileNameHash(std::string(attributes[1].value).append("0")), VarMap())).first; //for globals. Makes a bad assumption about where globals are. Fix.
    }
    /**
     * startElementNs
     * @param localname the name of the element tag
     * @param prefix the tag prefix
     * @param URI the namespace of tag
     * @param nb_namespaces number of namespaces definitions
     * @param namespaces the defined namespaces
     * @param nb_attributes the number of attributes on the tag
     * @param nb_defaulted the number of defaulted attributes
     * @param attributes list of attribute name value pairs (localname/prefix/URI/value/end)
     *
     * SAX handler function for start of an element.
     * Overide for desired behaviour.
    */
    virtual void startElement(const char * localname, const char * prefix, const char * URI,
                                int num_namespaces, const struct srcsax_namespace * namespaces, int num_attributes,
                                const struct srcsax_attribute * attributes) {
          
        unsigned int lineNum = 0;
        if(num_attributes){
            lineNum = strtoul(attributes[0].value, NULL, 0);
        }
        std::string lname(localname);
        if(lname == "decl_stmt"){
            currentNameAndLine.first.clear();
            ++triggerField[decl_stmt];
        }else if(lname == "function" || lname == "constructor" || lname == "destructor"){
            if(lname == "constructor"){
                ++constructorNum;//constructors have numbers appended to them since they all have the same name.
                isConstructor = true;
            }
            currentNameAndLine.first.clear();
            ++triggerField[function];
        }else if (lname == "expr_stmt"){
            currentNameAndLine.first.clear();
            ++triggerField[expr_stmt];
        }else if (lname == "parameter_list"){
            currentNameAndLine.first.clear();
            ++triggerField[parameter_list];
        }else if (lname == "argument_list"){
            if(triggerField[call]){
                std::size_t pos = currentNameAndLine.first.find(nameOfCurrentClldFcn);
                if(pos != std::string::npos){
                    //currentNameAndLine.first.erase(pos, nameOfCurrentClldFcn.size());
                }
            }
            ++triggerField[argument_list];
        }else if (lname == "call"){

            ++triggerField[call];
        }
        if(triggerField[decl_stmt] || triggerField[function] || triggerField[expr_stmt] || 
            triggerField[parameter_list] || triggerField[argument_list] || triggerField[call]){
            if (lname == "param"){
                    currentNameAndLine.first.clear();
                    ++triggerField[param];
            }else if(lname == "member_list"){
                    ++triggerField[member_list];
            }else if(lname == "class"){
                    ++triggerField[classn];
            }else if(lname == "index"){
                    ++triggerField[index];
            }else if(lname == "operator"){
                    ++triggerField[op];
            }else if (lname == "block"){ //So we can discriminate against things in or outside of blocks
                    currentNameAndLine.first.clear();
                    ++triggerField[block];
            }else if (lname == "init"){//so that we can get more stuff after the decl's name
                    currentNameAndLine.first.clear(); 
                    ++triggerField[init];
            }else if (lname == "argument"){
                    ++numArgs;
                    currentNameAndLine.first.append(":"); //denote arguments so that I can parse them out later.
                    if(triggerField[init] || triggerField[call]){
                        //Only if we're in init because templates can have arguments too
                        //currentNameAndLine.first.clear();
                        //currentNameAndLine.first.clear(); //so function arguments and the name won't ram one another
                    }
                    ++triggerField[argument];
            }else if (lname == "literal"){
                    ++triggerField[literal];
            }else if (lname == "modifier"){
                    ++triggerField[modifier];
            }else if(lname == "decl"){
                    ++triggerField[decl]; 
            }else if(lname == "type"){
                    ++triggerField[type]; 
            }else if (lname == "expr"){
                    ++triggerField[expr];
            }else if (lname == "name"){
                ++triggerField[name];
                currentNameAndLine.second = lineNum;
            }else{
                    ++triggerField[nonterminal]; 
            }   
        }
    }
    /**
     * charactersUnit
     * @param ch the characers
     * @param len number of characters
     *
     * SAX handler function for character handling within a unit.
     * Overide for desired behaviour.
     */
    virtual void charactersUnit(const char * ch, int len) {
        std::string content = "";
        content.append(ch, len);

        /*This has two cases I don't particularly like and am certain aren't needed (content != "=" and content != "new").
         *Fix at some point. For now, this solution works.*/
        if((triggerField[name] || triggerField[op]) && content != "new" && !triggerField[index]){
            currentNameAndLine.first.append(content);
            //std::cerr<<currentNameAndLine.first<<std::endl;
        }
    }

    // end elements may need to be used if you want to collect only on per file basis or some other granularity.
    virtual void endRoot(const char * localname, const char * prefix, const char * URI) {

    }
    virtual void endUnit(const char * localname, const char * prefix, const char * URI) {

    }
    virtual void endElement(const char * localname, const char * prefix, const char * URI) {
        std::string lname(localname);
        if(lname == "decl_stmt"){
            ProcessDeclStmtInit();

            currentNameAndLine.first.clear();
            functionTmplt.declstmt.clear();
            --triggerField[decl_stmt];
        }else if (lname == "expr_stmt"){
            ProcessExprStmt();
 
            --triggerField[expr_stmt];

            currentNameAndLine.first.clear();
            functionTmplt.exprstmt.clear();
            
        }else if(lname == "function" || lname == "constructor" || lname == "destructor"){
            sysDict.functionTable.insert(std::make_pair(functionTmplt.functionNumber, functionTmplt));
            FunctionIt = FileIt->second.insert(std::make_pair(functionTmplt.functionNumber, VarMap())).first;
            if(lname == "constructor"){
                isConstructor = false;
            }
            functionTmplt.clear();
            currentNameAndLine.first.clear();
            --triggerField[function];
        }else if (lname == "parameter_list"){
            --triggerField[parameter_list];
        }else if (lname == "argument_list"){
            numArgs = 0;
            --triggerField[argument_list];
        }else if (lname == "call"){
            isNestedNameInCall = false; //to tell me if I'm in a nested name because if I am I have to do special stuff. Only for calls.
            nameOfCurrentClldFcn = "";
            --triggerField[call];
        }
        if(triggerField[decl_stmt] || triggerField[function] || triggerField[expr_stmt] 
            || triggerField[parameter_list] || triggerField[argument_list] || triggerField[call]){
            if (lname == "param"){
                --triggerField[param];
            }else if (lname == "literal"){
                if(triggerField[expr_stmt]){
                    functionTmplt.exprstmt.rhs = currentNameAndLine.first;
                }
                --triggerField[literal];
            }else if(lname == "class"){
                    --triggerField[classn];
            }else if(lname == "member_list"){
                    --triggerField[member_list];
            }else if(lname == "index"){
                    --triggerField[index];
            }else if (lname == "modifier"){
                if(triggerField[decl_stmt] && triggerField[decl]){ //only care about modifiers in decls
                    functionTmplt.declstmt.potentialAlias = true;
                }else if(triggerField[function] && triggerField[parameter_list] && triggerField[param] && triggerField[decl]){
                    functionTmplt.arg.potentialAlias = true;
                }
                --triggerField[modifier];
            }else if (lname == "argument"){

                --triggerField[argument];
            }else if (lname == "block"){
                --triggerField[block];
            }else if(lname == "decl"){
                --triggerField[decl];                 
            }else if(lname == "init"){
                --triggerField[init];                 
            }else if (lname == "expr"){
                if(triggerField[decl_stmt] && triggerField[decl] && triggerField[init] && 
                !(triggerField[type] || triggerField[argument_list] || triggerField[call])){ //do something if there's an init
                    //std::cerr<<"Init: "<<currentNameAndLine.first<<std::endl;
                    auto expr = SplitLhsRhs(currentNameAndLine.first);
                    std::cerr<<"GO: "<<expr.front()<<" : "<<expr.back()<<std::endl;
                    if(expr.size() > 1){ //found an expression of the form lhs = rhs. Make a new lhs slice profile and then iterate over rhs to insert.
                        auto strVec = Split(expr.back(), "+<.*->&=(),"); //split rhs
                        //std::cerr<<"Prof: " <<  functionTmplt.functionName+":"+expr.front()<<std::endl;
                        varIt = FunctionIt->second.insert(std::make_pair(functionTmplt.functionName+":"+expr.front(), 
                            SliceProfile(functionTmplt.declstmt.ln - functionTmplt.functionLineNumber, fileNumber, 
                                functionTmplt.functionNumber, currentNameAndLine.second, 
                                functionTmplt.functionName+":"+expr.front(), functionTmplt.declstmt.potentialAlias))).first;
                        for(std::string str : strVec){
                            if(str != expr.front()){
                                //std::cerr<<"Name: "<<functionTmplt.functionName+":"+str<<std::endl;
                                auto sp = FunctionIt->second.find(functionTmplt.functionName+":"+str);
                                if(sp != FunctionIt->second.end()){
                                    varIt->second.slines.insert(currentNameAndLine.second);
                                    sp->second.slines.insert(currentNameAndLine.second);
                                    if(varIt->second.potentialAlias){
                                        varIt->second.aliases.insert(functionTmplt.functionName+":"+str);
                                    }else{
                                        sp->second.dvars.insert(varIt->second.variableName);
                                    }
                                }
                            }

                        }
                    }else{ //found an expression on the rhs of something. Because lhs gets strings first, we need to process that.
                        auto strVec = Split(expr.front(), "+<:.*->&=(),");
                        for(std::string str : strVec){
                            //std::cerr<<"Name: "<<functionTmplt.functionName+":"+str<<std::endl;
                            auto sp = FunctionIt->second.find(functionTmplt.functionName+":"+str);
                            if(sp != FunctionIt->second.end()){
                                varIt->second.slines.insert(currentNameAndLine.second);
                                sp->second.slines.insert(currentNameAndLine.second);
                                if(varIt->second.potentialAlias){
                                    varIt->second.aliases.insert(functionTmplt.functionName+":"+str);
                                }else{
                                    sp->second.dvars.insert(functionTmplt.functionName+":"+varIt->second.variableName);
                                }
                            }
                        }
                    }
                    currentNameAndLine.first.clear(); //because if it's a multi-init decl then inits will run into one another.
                    //ProcessDeclStmtInit();
                    //
                }else if(triggerField[decl_stmt] && triggerField[decl] && triggerField[argument_list] && triggerField[argument] && triggerField[expr] &&
                        !triggerField[type]){ //For the case where we need to get a constructor decl
                        auto strVec = Split(currentNameAndLine.first, "+<.*->&=():,");
                        for(std::string str : strVec){
                            auto sp = FunctionIt->second.find(functionTmplt.functionName+":"+str);
                            if(sp != FunctionIt->second.end()){
                                varIt->second.dvars.insert(sp->second.variableName);
                                std::cerr<<"This: "<<str<<std::endl;
                            }
                        }
                }
                --triggerField[expr]; 
            }else if (lname == "type"){
                if(triggerField[decl_stmt] || (triggerField[function] && ! triggerField[block])) {
                    currentNameAndLine.first.clear();
                }
                --triggerField[type]; 
            }else if (lname == "operator"){
                --triggerField[op];
            }
            else if (lname == "name"){
                //Get Function names and arguments
                if(triggerField[function]){
                    GetFunctionData();
                }
                //Get variable decls
                if(triggerField[decl_stmt]){
                    GetDeclStmtData();
                }
                if(triggerField[expr_stmt]){
                    GetExprStmtParts();
                }
                
                //Name of function being called
                if(triggerField[call] && triggerField[name] && !(triggerField[argument_list])){
                    if(triggerField[name] > 1){ //have seen more than one name tag. In a complex name.
                        isNestedNameInCall = true;
                    }
                    //quirky and not a great way to do this. Fix. Primary issue is that it relies on too many happenstances.
                    if(triggerField[name] == 2 || isNestedNameInCall == false){
                        auto spltVec = SplitLhsRhs(currentNameAndLine.first);
                        nameOfCurrentClldFcn = spltVec.back();
                        //currentNameAndLine.first.clear();
                        //std::cerr<<"CALLED"<<spltVec.back();
                    }
                }
                //Get function arguments
                if(triggerField[call]){
                    GetCallData();
                }
                --triggerField[name];

            }
        }
    }

    /**
     * GetCallData
     *
     * Knows the proper constrains for obtaining the name of arguments of currently called function
     * It stores data about those variables if it can find a slice profile entry for them.
     * Essentially, update the slice profile of the argument to reflect new data.
     */
    void GetCallData(){
        //Get function arguments
        if(triggerField[argument_list] && triggerField[argument] && 
            triggerField[expr] && triggerField[name]){
            auto strVec = Split(currentNameAndLine.first, ":+<.*->&=(),");
            for(std::string str : strVec){
                //std::cerr<<"Attempt: "<<functionTmplt.functionName<<":"<<str<<std::endl;
                auto sp = FunctionIt->second.find(functionTmplt.functionName+":"+str);
                if(sp != FunctionIt->second.end()){
                    sp->second.slines.insert(currentNameAndLine.second);
                    sp->second.index = currentNameAndLine.second - functionTmplt.functionLineNumber;
                    sp->second.cfunctions.push_back(std::make_pair(nameOfCurrentClldFcn, numArgs));
                    std::size_t pos = currentNameAndLine.first.rfind(str);
                    if(pos != std::string::npos){
                        currentNameAndLine.first.erase(pos, str.size()); //remove the argument from the string so it won't concat with the next
                    }
                }
            }
        }
    }
    /**
     * GetFunctionData
     * Knows proper constraints for obtaining function's return type, name, and arguments. Stores all of this in
     * functionTmplt.
     */
    void GetFunctionData(){
        //Get function type
        if(triggerField[type] && !(triggerField[parameter_list] || triggerField[block] || triggerField[member_list])){
            functionTmplt.returnType = currentNameAndLine.first;
        }
        //Get function name
        if(triggerField[name] == 1 && !(triggerField[argument_list] || 
            triggerField[block] || triggerField[type] || triggerField[parameter_list] || triggerField[member_list])){
            
            std::size_t pos = currentNameAndLine.first.find("::");
            if(pos != std::string::npos){
                currentNameAndLine.first.erase(0, pos+2);
            }
            if(isConstructor){
                std::stringstream ststrm;
                ststrm<<constructorNum;
                currentNameAndLine.first+=ststrm.str(); //number the constructor. Find a better way than stringstreams someday.
                
            }else{
                //std::cerr<<"NAME: "<<currentNameAndLine.first<<std::endl;
            }
            if(functionTmplt.functionName.empty()){
                functionTmplt.functionName = currentNameAndLine.first;  //to defend against general weirdness in srcML. Remember to clear at /function
            }

            functionTmplt.functionLineNumber = currentNameAndLine.second;
            functionTmplt.functionNumber = functionAndFileNameHash(currentNameAndLine.first); //give me the hash num for this name.
            //std::cerr<<"Func: "<<functionTmplt.functionName<<std::endl;
            currentNameAndLine.first.clear(); //saved the function's name. Get rid of it from the temp string.
            //std::cerr<<currentNameAndLine.first<<std::endl;
            
        }
        //Get param types
        if(triggerField[parameter_list] && triggerField[param] && triggerField[decl] && triggerField[type] && !triggerField[block]){
            functionTmplt.arg.type = currentNameAndLine.first;
            //functionTmplt.arguments.push_back(std::make_pair(currentNameAndLine.first, ""));
        }
        //Get Param names
        if(triggerField[parameter_list] && triggerField[param] && triggerField[decl] && !(triggerField[type] || triggerField[block])){

            functionTmplt.arg.name = currentNameAndLine.first;
            functionTmplt.arg.ln = currentNameAndLine.second;
            functionTmplt.arguments.push_back(functionTmplt.arg);
            
            //std::cerr<<"Param: "<<functionTmplt.arg.type <<" "<<functionTmplt.functionName+":"+functionTmplt.arg.name<<std::endl;
            
            varIt = FunctionIt->second.insert(std::make_pair(functionTmplt.functionName+":"+functionTmplt.arg.name, 
                SliceProfile(functionTmplt.arg.ln - functionTmplt.functionLineNumber, fileNumber, 
                    functionTmplt.functionNumber, functionTmplt.arg.ln, functionTmplt.arg.name, functionTmplt.arg.potentialAlias))).first;
        }

        
    }
    /**
     * GetDeclStmtData
     * Knows proper constraints for obtaining DeclStmt type and name.
     * creates a new slice profile and stores data about decle statement inside.
     */
    void GetDeclStmtData(){
        if(triggerField[decl] && triggerField[type] && !(triggerField[init])){
            //std::cerr<<currentNameAndLine.first<<std::endl;
            functionTmplt.declstmt.type = currentNameAndLine.first;
            //agglomerate string into type. Clear when we exit the decl_stmt
        }
        //Get name of decl stmt
        if(triggerField[decl] && !(triggerField[type] || triggerField[init] || triggerField[expr] || triggerField[index] || triggerField[classn])){
            functionTmplt.declstmt.name = currentNameAndLine.first;
            functionTmplt.declstmt.ln = currentNameAndLine.second;
            if(functionTmplt.declstmt.name[0] == ','){//corner case with decls like: int i, k, j. This is a patch, fix properly later.
                functionTmplt.declstmt.name.erase(0,1);
            }
            //std::cout<<"Name: "<<functionTmplt.functionName+":"+functionTmplt.declstmt.name<<std::endl;
            varIt = FunctionIt->second.insert(std::make_pair(functionTmplt.functionName+":"+functionTmplt.declstmt.name, 
                SliceProfile(functionTmplt.declstmt.ln - functionTmplt.functionLineNumber, fileNumber, 
                    functionTmplt.functionNumber, functionTmplt.declstmt.ln, 
                    functionTmplt.declstmt.name, functionTmplt.declstmt.potentialAlias))).first;
            currentNameAndLine.first.clear();
        }

        //Get Init of decl stmt
    }
    /**
     * GetExorStmtParts
     * Knows proper constraints for obtaining exprstmts left and (if applicable) right hand side
     * this data is carried to a secondary function called ProcessExprSmtRhs
     */
    void GetExprStmtParts(){

    }
    /**
     * ProcessDecleStmtInit
     * Knows proper constraints for obtaining data bout the initialization part of a declstmt
                 //std::cerr<<"Tried: "<<*rVecIt<<" "<<functionTmplt.declstmt.name<<std::endl;
            if(functionTmplt.declstmt.name != *rVecIt){ //lhs != rhs
                auto sp = FunctionIt->second.find(functionTmplt.functionName+":"+ *rVecIt);
                if(sp != FunctionIt->second.end()){
                    //Either has to be a variable local to this function or a global variable (function == 0)
                    //std::cerr<<"Found: "<<functionTmplt.declstmt.name<<" "<<functionTmplt.functionName+":"+*rVecIt<<" "<<sp->second.function<<" "<<functionTmplt.functionNumber<<" "<<functionTmplt.functionName+":"+<<std::endl;
                    if(sp->second.function == functionTmplt.functionNumber || sp->second.function == 0){
                        std::string fdname(functionTmplt.functionName+":"+functionTmplt.declstmt.name);
                        auto lastSp = FunctionIt->second.find(fdname);
                        sp->second.slines.insert(currentNameAndLine.second);
                        if(lastSp != FunctionIt->second.end() && !lastSp->second.potentialAlias){ 
                            //std::cerr<<"dat: "<<*rVecIt<<" "<<functionTmplt.declstmt.name<<" "<<sp->second.function<<" "<<functionTmplt.functionNumber<<std::endl;
                            sp->second.dvars.insert(functionTmplt.functionName+":"+functionTmplt.declstmt.name);
                        }else if(lastSp != FunctionIt->second.end() && sp->second.potentialAlias && lastSp->second.potentialAlias){
                            //it's an alias for the rhs.
                            sp->second.isAlias = true;
                            sp->second.lastInsertedAlias = sp->second.aliases.insert(fdname).first;                            
                        }
                    }
                }
            }
     */
    void ProcessDeclStmtInit(){
        auto resultVec = Split(currentNameAndLine.first, "+<.*->&=(),");
        for(auto rVecIt = resultVec.begin(); rVecIt != resultVec.end(); ++rVecIt){
            //std::cerr<<"Stuff: "<<*rVecIt<<std::endl;
        }

    }
    void ProcessExprStmt(){
        //std::cerr<<"Curr"<<currentNameAndLine.first<<std::endl;
        auto lhsrhs = SplitLhsRhs(currentNameAndLine.first);
        auto resultVecl = Split(lhsrhs.front(), "+<.*->&");
        VarMap::iterator splIt;
        //First, take the left hand side and mark sline information. Doing it first because later I'll be iterating purely
        //over the rhs.
        for(auto rVecIt = resultVecl.begin(); rVecIt!= resultVecl.end(); ++rVecIt){
            //std::cerr<<"LHS: "<<functionTmplt.functionName+":"+*rVecIt<<std::endl;
            splIt = FunctionIt->second.find(functionTmplt.functionName+":"+*rVecIt);
            if(splIt != FunctionIt->second.end()){ //Found it so add statement line.
                splIt->second.slines.insert(currentNameAndLine.second);
                break; //found it, don't care about the rest (ex. in: bottom -> next -- all I need is bottom.)
            }            
        }
        //std::cerr<<"MM: "<<currentNameAndLine.first<<std::endl;
        //Doing rhs now. First check to see if there's anything to process (Note: Check to make sure Op even needs to be here anymore)
        if(!(splIt == FunctionIt->second.end())){
            auto resultVecr = Split(lhsrhs.back(), "+<.*->&"); //Split on tokens
            
            for(auto rVecIt = resultVecr.begin(); rVecIt != resultVecr.end(); ++rVecIt){ //loop over words and check them against map
                std::string fullName = functionTmplt.functionName+":"+*rVecIt;
                
                if(splIt->first != fullName){//lhs !+ rhs
                    auto sprIt = FunctionIt->second.find(fullName);//find the sp for the rhs
                    if(sprIt != FunctionIt->second.end()){ //lvalue depends on this rvalue
                        if(!(splIt == FunctionIt->second.end() && splIt->second.potentialAlias)){
                            sprIt->second.dvars.insert(splIt->first); //it's not an alias so it's a dvar
                        }else{//it is an alias, so save that this is the most recent alias and insert it into rhs alias list
                            splIt->second.isAlias = true;
                            sprIt->second.lastInsertedAlias = sprIt->second.aliases.insert(splIt->first).first;
                        }
                        sprIt->second.slines.insert(currentNameAndLine.second);                            
                        if(sprIt->second.isAlias){//Union things together. If this was an alias of anoter thing, update the other thing
                            auto spaIt = FunctionIt->second.find(*sprIt->second.lastInsertedAlias);
                            if(spaIt != FunctionIt->second.end()){
                                spaIt->second.dvars.insert(splIt->first);
                                spaIt->second.slines.insert(currentNameAndLine.second);
                            }
                        }
                    }
                }
            }
        }
    }

#pragma GCC diagnostic pop

};

#endif
