#pragma once

#include <monad/mpt/config.hpp>

#include <functional>

MONAD_MPT_NAMESPACE_BEGIN

class Node;
class UpdateAux;

struct TraverseMachine
{
    virtual void down(unsigned char branch, Node const &) = 0;
    virtual void up(unsigned char branch, Node const &) = 0;
};

void preorder_traverse(UpdateAux &, Node const &, TraverseMachine &);

MONAD_MPT_NAMESPACE_END
