#ifndef PARSER_CALLBACK_HPP
#define PARSER_CALLBACK_HPP

#include "osmobject.hpp"
#include "node.hpp"
#include "way.hpp"
#include "relation.hpp"
#include "types.hpp"


class Parser_Callback {

public:

	virtual void node(const Node&, operation op, bool if_unused) = 0;

	virtual void way(const Way&, operation op, bool if_unused) = 0;

	virtual void relation(const Relation&, operation op, bool if_unused) = 0;

};

#endif
