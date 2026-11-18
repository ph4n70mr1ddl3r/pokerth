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

// Regression test: ConfigFile::fillBuffer() reads list config values from the
// wrong XML depth, silently discarding every list on every startup.
//
// Background
// ----------
// ConfigFile::writeBuffer() (src/config/configfile.cpp:521-541) serializes list
// config variables like this:
//
//   <Configuration>
//     <ManualBlindsList value="" type="list">
//       <ManualBlindsList value="10"/>
//       <ManualBlindsList value="20"/>
//     </ManualBlindsList>
//   </Configuration>
//
// The outer element carries type="list"; its children are leaf elements that
// only carry a "value" attribute.
//
// The bug
// -------
// ConfigFile::fillBuffer() (src/config/configfile.cpp:444-449) reads the list
// with one extra firstChildElement() call:
//
//   QDomElement confList = xmlDoc.documentElement()
//       .firstChildElement("Configuration")
//       .firstChildElement(name)
//       .firstChildElement();                  // <-- extra descent
//   for (QDomElement n = confList.firstChildElement(); ...)
//       tempStringList2.push_back(n.attribute("value").toStdString());
//
// That extra descent lands on the FIRST LEAF item (e.g. the element carrying
// value="10"), not the outer list element. The loop then iterates the children
// of that leaf -- of which there are none -- so tempStringList2 stays empty and
// line 451 overwrites configBufferList[i].defaultListValue with an empty list.
// readConfigStringList()/readConfigIntList() then hand that empty list to every
// consumer (ignore list, manual blinds, info-message filter, bad-word lists,
// etc.). A subsequent writeBuffer() persists the empty lists, making the loss
// permanent.
//
// The migration path in the SAME file (configfile.cpp:686-688) reads the
// identical structure correctly, with no trailing firstChildElement(), which
// proves the extra descent is a typo/regression.
//
// The fix
// -------
// Iterate the children of the outer list element directly (the `conf` element
// already resolved at line 430), dropping the erroneous extra descent.
//
// This standalone test models the DOM tree produced by writeBuffer() using
// std types and compares the buggy reader with the fixed reader (which mirrors
// the correct migration reader). It asserts the bug is reproducible and the fix
// restores all list values while leaving scalar reads untouched.
//
// Compile:
//   g++ -O2 -std=c++23 -Wall -Wextra -o /tmp/test_configfile_list_read_bug test_configfile_list_read_bug.cpp
//   /tmp/test_configfile_list_read_bug

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

    // Iterate element children via nextSiblingElement(), matching the Qt loop.
    std::vector<std::string> childValues() const
    {
        std::vector<std::string> out;
        for (const Node &n : children) {
            out.push_back(n.value);
        }
        return out;
    }
};

static Node scalarNode(const std::string &name, const std::string &value)
{
    Node n;
    n.name = name;
    n.value = value;
    return n;
}

// Build the exact structure emitted by writeBuffer() for a list variable.
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
    root.name = "PokerTH";
    Node config;
    config.name = "Configuration";
    config.children = vars;
    root.children.push_back(config);
    return root;
}

// Verbatim model of the buggy fillBuffer() path (configfile.cpp:444-449).
static std::vector<std::string> readListBuggy(const Node &root, const std::string &name)
{
    std::vector<std::string> result;
    const Node *config = root.firstChildElement("Configuration");
    if (!config) {
        return result;
    }
    const Node *outer = config->firstChildElement(name);
    if (!outer) {
        return result;
    }
    const Node *confList = outer->firstChildElement(); // BUG: extra descent to first leaf
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
// This mirrors the correct migration reader at configfile.cpp:686-688.
static std::vector<std::string> readListFixed(const Node &root, const std::string &name)
{
    std::vector<std::string> result;
    const Node *config = root.firstChildElement("Configuration");
    if (!config) {
        return result;
    }
    const Node *outer = config->firstChildElement(name);
    if (!outer) {
        return result;
    }
    for (const Node *n = outer->firstChildElement(); n != nullptr;
         n = n->nextSibling(outer->children)) {
        result.push_back(n->value);
    }
    return result;
}

static bool same(const std::vector<std::string> &a, const std::vector<std::string> &b)
{
    return a == b;
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
        listNode("ManualBlindsList", {"10", "20", "30"}),
        listNode("IfInfoMessageShowList", {"new_game", "game_finished"}),
        listNode("Singleton", {"only"}),
        listNode("EmptyList", {}),
        scalarNode("MyName", "Human Player"),
    });

    struct Case {
        const char *name;
        std::vector<std::string> expected;
    };
    const std::vector<Case> cases = {
        {"ManualBlindsList", {"10", "20", "30"}},
        {"IfInfoMessageShowList", {"new_game", "game_finished"}},
        {"Singleton", {"only"}},
        {"EmptyList", {}},
    };

    printf("Testing ConfigFile::fillBuffer() list-read bug:\n\n");

    for (const Case &tc : cases) {
        const std::vector<std::string> buggy = readListBuggy(root, tc.name);
        const std::vector<std::string> fixed = readListFixed(root, tc.name);

        printf("Variable %s (expected ", tc.name);
        printVec("", tc.expected);
        printVec("current", buggy);
        printVec("fixed  ", fixed);

        if (!same(fixed, tc.expected)) {
            printf("  ^ FIXED MISMATCH\n");
            ++failures;
        }
        if (!same(buggy, tc.expected)) {
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
    const Node *myName = config->firstChildElement("MyName");
    if (!myName || myName->value != "Human Player") {
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
