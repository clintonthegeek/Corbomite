// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/search/SearchAst.h"

namespace Corbomite {

SearchNodePtr SearchNode::makeText(QString t)
{
    auto n = std::make_shared<SearchNode>();
    n->kind = Kind::Text;
    n->text = std::move(t);
    return n;
}

SearchNodePtr SearchNode::makePhrase(QString t)
{
    auto n = std::make_shared<SearchNode>();
    n->kind = Kind::Phrase;
    n->text = std::move(t);
    return n;
}

SearchNodePtr SearchNode::makeRegex(QString pattern)
{
    auto n = std::make_shared<SearchNode>();
    n->kind = Kind::Regex;
    n->text = std::move(pattern);
    return n;
}

SearchNodePtr SearchNode::makeAnd(QVector<SearchNodePtr> children)
{
    auto n = std::make_shared<SearchNode>();
    n->kind = Kind::And;
    n->children = std::move(children);
    return n;
}

SearchNodePtr SearchNode::makeOr(QVector<SearchNodePtr> children)
{
    auto n = std::make_shared<SearchNode>();
    n->kind = Kind::Or;
    n->children = std::move(children);
    return n;
}

SearchNodePtr SearchNode::makeNot(SearchNodePtr child)
{
    auto n = std::make_shared<SearchNode>();
    n->kind = Kind::Not;
    n->children.append(std::move(child));
    return n;
}

SearchNodePtr SearchNode::makeGroup(SearchNodePtr child)
{
    auto n = std::make_shared<SearchNode>();
    n->kind = Kind::Group;
    n->children.append(std::move(child));
    return n;
}

SearchNodePtr SearchNode::makeOpCall(QString name, SearchNodePtr operand)
{
    auto n = std::make_shared<SearchNode>();
    n->kind = Kind::OpCall;
    n->text = std::move(name);
    n->children.append(std::move(operand));
    return n;
}

SearchNodePtr SearchNode::makeProperty(SearchNodePtr key, SearchNodePtr value)
{
    auto n = std::make_shared<SearchNode>();
    n->kind = Kind::PropertyCall;
    n->children.append(std::move(key));
    if (value) n->children.append(std::move(value));
    return n;
}

SearchNodePtr SearchNode::makePropertyValue(QString keyword)
{
    auto n = std::make_shared<SearchNode>();
    n->kind = Kind::PropertyValue;
    n->text = std::move(keyword);
    return n;
}

SearchNodePtr SearchNode::makeCompare(QString op, SearchNodePtr operand)
{
    auto n = std::make_shared<SearchNode>();
    n->kind = Kind::Compare;
    n->text = std::move(op);
    n->children.append(std::move(operand));
    return n;
}

} // namespace Corbomite
