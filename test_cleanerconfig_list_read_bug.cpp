/*****************************************************************************
 * PokerTH - The open source texas holdem engine                             *
 * Copyright (C) 2006-2012 Felix Hammer, Florian Thauer, Lothar May          *
 *                                                                           *
 * This program is free software: you can redistribute it and/or modify      *
 * it under the terms of the GNU Affero General Public License as            *
 * published by the Free Software Foundation, either version 3 of the        *
 * License, or (at your option) any later version.                           *
 *                                                                           *
 * This program is distributed in the hope that it will be useful,           *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of            *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
 * GNU Affero General Public License for more details.                       *
 *                                                                           *
 * You should have received a copy of the GNU Affero General Public License  *
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.     *
 *****************************************************************************/

// Regression test: CleanerConfig::fillBuffer() reads list config values from
// the wrong XML depth, silently discarding every chat-cleaner list on load.
//
// Background
// ----------
// CleanerConfig::writeBuffer() (src/chatcleaner/cleanerconfig.cpp:304-316)
// serializes list config variables like this:
//
//   <Configuration>
//     <BadWordsList value="" type="list">
//       <BadWordsList value="arsch"/>
//       <BadWordsList value="asshole"/>
//     </BadWordsList>
//   </Configuration>
//
// The outer element carries type="list"; its children are leaf elements that
// only carry a "value" attribute.
//
// The bug
// -------
// CleanerConfig::fillBuffer() (src/chatcleaner/cleanerconfig.cpp:270-275)
// reads the list with one extra firstChildElement() call:
//
//   QDomElement confList = xmlDoc.documentElement()
//       .firstChildElement("Configuration")
//       .firstChildElement(name)
//       .firstChildElement();                  // <-- extra descent
//   for (QDomElement n = confList.firstChildElement(); ...)
//       tempStringList2.push_back(n.attribute("value").toStdString());
//
// That extra descent lands on the FIRST LEAF item (e.g. the element carrying
// value="arsch"), not the outer list element. The loop then iterates the
// children of that leaf -- of which there are none -- so tempStringList2 stays
// empty and line 277 overwrites configBufferList[i].defaultListValue with an
// empty list.
//
// This affects all four shipped list settings -- BadWordsList,
// BadWordsException, UrlStringsList and UrlExceptionStringsList. On each
// cleaner startup MessageFilter::refreshConfig() then hands empty word/URL
// lists to BadWordCheck and UrlCheck, so the chat cleaner silently filters
// nothing. A subsequent writeBuffer() persists the empty lists, making the
// loss permanent.
//
// The migration path in the SAME file (cleanerconfig.cpp:426-428) reads the
// identical structure correctly, with no trailing firstChildElement(), which
// proves the extra descent is a typo/regression. This is the CleanerConfig
// twin of the ConfigFile::fillBuffer() bug fixed in commit aebd7c3.
//
// The fix
// -------
// Iterate the children of the outer list element directly (the `conf` element
// already resolved at line 260), dropping the erroneous extra descent.
//
// This standalone test models the DOM tree produced by writeBuffer() using
// std types and compares the buggy reader with the fixed reader (which mirrors
// the correct migration reader). It asserts the bug is reproducible and the
// fix restores all list values while leaving scalar reads untouched.
//
// Compile:
//   g++ -O2 -std=c++23 -Wall -Wextra -o /tmp/test_cleanerconfig_list_read_bug test_cleanerconfig_list_read_bug.cpp
//   /tmp/test_cleanerconfig_list_read_bug

#include <cstdio>
#include <string>
#include <vector>

// Minimal model of the subset of QDomElement used by the config readers.
struct Node {
    std::string name;
    std::string value;
    std::string type;
    std::vector<Node> children;

    bool isNull() const { return name.empty(); }

    const Node *firstChildElement() const
    {
        return children.empty() ? nullptr : &children.front();
    }

    const Node *firstChildElement(const std::string &n) const
    {
        for (const Node &c : children) {
            if (c.name == n) {
                return &c;
            }
        }
        return nullptr;
    }

    const Node *nextSibling(const std::vector<Node> &siblings) const
    {
        for (size_t i = 0; i < siblings.size(); ++i) {
            if (&siblings[i] == this) {
                return (i + 1 < siblings.size()) ? &siblings[i + 1] : nullptr;
            }
        }
        return nullptr;
    }
};

static Node scalarNode(const std::string &name, const std::string &value)
{
    Node n;
    n.name = name;
    n.value = value;
    return n;
}

// Build the exact structure emitted by CleanerConfig::writeBuffer() for a list
// variable: an outer element with type="list" whose children are leaf elements
// carrying only a "value" attribute.
static Node listNode(const std::string &name, const std::vector<std::string> &values)
{
    Node outer;
    outer.name = name;
    outer.type = "list";
    for (const std::string &v : values) {
        outer.children.push_back(scalarNode(name, v));
    }
    return outer;
}

static Node buildConfig(const std::vector<Node> &vars)
{
    Node root;
    root.name = "PokerTHCleaner";
    Node config;
    config.name = "Configuration";
    config.children = vars;
    root.children.push_back(config);
    return root;
}

// Verbatim model of the buggy fillBuffer() path (cleanerconfig.cpp:260-277).
static std::vector<std::string> readListBuggy(const Node &root, const std::string &name)
{
    std::vector<std::string> result;
    const Node *config = root.firstChildElement("Configuration");
    if (!config) {
        return result;
    }
    const Node *conf = config->firstChildElement(name);
    if (!conf || conf->type != "list") {
        return result;
    }
    const Node *confList = conf->firstChildElement(); // BUG: extra descent to first leaf
    if (!confList) {
        return result;
    }
    for (const Node *n = confList->firstChildElement(); n != nullptr;
         n = n->nextSibling(confList->children)) {
        result.push_back(n->value);
    }
    return result;
}

// Fixed reader: iterate the children of the outer list element directly.
// This mirrors the correct migration reader at cleanerconfig.cpp:426-428.
static std::vector<std::string> readListFixed(const Node &root, const std::string &name)
{
    std::vector<std::string> result;
    const Node *config = root.firstChildElement("Configuration");
    if (!config) {
        return result;
    }
    const Node *conf = config->firstChildElement(name);
    if (!conf || conf->type != "list") {
        return result;
    }
    for (const Node *n = conf->firstChildElement(); n != nullptr;
         n = n->nextSibling(conf->children)) {
        result.push_back(n->value);
    }
    return result;
}

static void printVec(const char *label, const std::vector<std::string> &v)
{
    printf("  %s: [", label);
    for (size_t i = 0; i < v.size(); ++i) {
        printf("%s\"%s\"", i ? ", " : "", v[i].c_str());
    }
    printf("]\n");
}

int main()
{
    int failures = 0;
    int reproduced = 0;

    const Node root = buildConfig({
        listNode("BadWordsList", {"arsch", "asshole", "shit"}),
        listNode("BadWordsException", {" anal ", " arsch "}),
        listNode("UrlStringsList", {".com", ".net", "http://"}),
        listNode("UrlExceptionStringsList", {"pokerth.net", "http://www.esl."}),
        listNode("EmptyList", {}),
        scalarNode("WarnLevelToKick", "2"),
    });

    struct Case {
        const char *name;
        std::vector<std::string> expected;
    };
    const std::vector<Case> cases = {
        {"BadWordsList", {"arsch", "asshole", "shit"}},
        {"BadWordsException", {" anal ", " arsch "}},
        {"UrlStringsList", {".com", ".net", "http://"}},
        {"UrlExceptionStringsList", {"pokerth.net", "http://www.esl."}},
        {"EmptyList", {}},
    };

    printf("Testing CleanerConfig::fillBuffer() list-read bug:\n\n");

    for (const Case &tc : cases) {
        const std::vector<std::string> buggy = readListBuggy(root, tc.name);
        const std::vector<std::string> fixed = readListFixed(root, tc.name);

        printf("Variable %s (expected ", tc.name);
        printVec("", tc.expected);
        printVec("current", buggy);
        printVec("fixed  ", fixed);

        if (fixed != tc.expected) {
            printf("  ^ FIXED MISMATCH\n");
            ++failures;
        }
        if (buggy != tc.expected) {
            ++reproduced;
        }
    }

    size_t nonEmptyCases = 0;
    for (const Case &tc : cases) {
        if (!tc.expected.empty()) {
            ++nonEmptyCases;
        }
    }

    // The scalar value must be read identically by both paths: the bug is
    // isolated to the list branch of fillBuffer().
    const Node *config = root.firstChildElement("Configuration");
    const Node *warnLevel = config->firstChildElement("WarnLevelToKick");
    if (!warnLevel || warnLevel->value != "2") {
        printf("Scalar value read mismatch\n");
        ++failures;
    }

    printf("\nNon-empty list variables misread by current logic: %d/%zu\n",
           reproduced, nonEmptyCases);
    if (static_cast<size_t>(reproduced) != nonEmptyCases) {
        printf("FAIL: expected all non-empty list cases to be misread\n");
        ++failures;
    }
    if (failures) {
        printf("\nTEST FAILED (%d failure(s))\n", failures);
        return 1;
    }
    printf("\nTEST PASSED: list-read bug reproduced and fix verified\n");
    return 0;
}
