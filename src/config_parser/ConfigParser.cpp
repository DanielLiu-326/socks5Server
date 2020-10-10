#include <iostream>
#include <fstream>
#include <assert.h>

#include "ConfigParser.h"
#include "Util.h"


CConfigParser::CConfigParser()
{

}

CConfigParser::~CConfigParser()
{
    auto it = m_mpConfigData.begin();
    for (; it != m_mpConfigData.end(); it++) {
        map<string, string> * pSection = it->second;
        if (pSection) {
            delete pSection;
            it->second = NULL;
        }
    }
    m_mpConfigData.clear();
}


bool CConfigParser::Parser(const string & cFilePath)
{
    ifstream input(cFilePath);
    if (!input) {
        return false;
    }
    try
    {
        string cCurSection="";
        map<string, string> *temp = new map<string, string>();
        m_mpConfigData.insert(make_pair("", temp));
        for (string line; getline(input, line); ) {
            if (line.find_first_of("#") != string::npos) {
                line.erase(line.find_first_of("#"));
            }
            if (line.empty()) continue;

            if (line[0] == '[')  {
                if (line[line.size()-1] != ']') {
                    throw runtime_error("section format error");
                }
                map<string, string> *pSection = new map<string, string>();
                if (pSection == NULL) {
                    throw runtime_error("run out of memory");
                }
                string sectionName = string(line, 1, line.size()- 2);
                m_mpConfigData.insert(make_pair(sectionName, pSection));
                cCurSection = sectionName; 
            } else {
                vector<string> kv = Split(line, "=");
                if(line.back()=='=')
                {
                    kv.push_back("");
                }
                if (kv.size() != 2) {
                    throw runtime_error("ini format error");
                }
                string k = Strim(kv[0], " ");
                string v = Strim(kv[1], " ");

                auto it = m_mpConfigData.find(cCurSection);
                assert(it != m_mpConfigData.end());
                it->second->insert(make_pair(k,v));
            }
        }
        input.close();
        return true;
    }
    catch(runtime_error e)
    {
        cout << e.what() << endl;
        auto it = m_mpConfigData.begin();
        for (; it != m_mpConfigData.end(); it++) {
            map<string, string> * pSection = it->second;
            if (pSection) {
                delete pSection;
                it->second = NULL;
            }
        }
        m_mpConfigData.clear();
        input.close();
        return false;
    }

}

bool CConfigParser::HasSection(const string & cSection)
{
    return m_mpConfigData.find(cSection) != m_mpConfigData.end() ? true : false;
}

int CConfigParser::GetSections(vector<string> & vecSections)
{
    for (auto it = m_mpConfigData.begin(); it !=  m_mpConfigData.end(); it++) {
        vecSections.push_back(it->first);
    }
    return 0;
}

int CConfigParser::GetKeys(const string & cSection, vector<string> & vecKeys)
{
    auto it = m_mpConfigData.find(cSection);
    if (it != m_mpConfigData.end()) {
        map<string, string> * pSection = it->second;
        for (auto it2 = pSection->begin(); it2 !=  pSection->end(); it2++) {
            vecKeys.push_back(it2->first);
        }
    }
    return 0;
}

map<string, string> *CConfigParser::GetSectionConfig(const string & cSection)
{
    auto it = m_mpConfigData.find(cSection);
    if (it != m_mpConfigData.end()) {
        return it->second;
    }
    return NULL;
}

string CConfigParser::GetConfig(const string & cSection, const string & cKey)
{
    const map<string, string> * pSection = GetSectionConfig(cSection);
    if (pSection) {
        auto it = pSection->find(cKey);
        if (it != pSection->end()) {
            return it->second;
        }
    }
    return "";
}

string CConfigParser::GetDefConfig(const string & cSection, const string & cKey, const string & cDef)
{
    const map<string, string> * pSection = GetSectionConfig(cSection);
    if (pSection) {
        auto it = pSection->find(cKey);
        if (it != pSection->end()) {
            return it->second;
        }
    }
    return cDef;
}

bool CConfigParser::HasKey(const string &cSection, const string &cKey) {
    if(this->m_mpConfigData.count(cSection))
    {
        return this->m_mpConfigData[cSection]->count(cKey)!=0;
    }
    else
    {
        return false;
    }
}

void CConfigParser::Clear() {
    for(auto x:m_mpConfigData)
    {
        delete x.second;
    }
    this->m_mpConfigData.clear();
}

