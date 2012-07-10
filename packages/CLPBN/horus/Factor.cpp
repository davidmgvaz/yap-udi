#include <cstdlib>
#include <cassert>

#include <algorithm>

#include <iostream>
#include <sstream>

#include "Factor.h"
#include "Indexer.h"


Factor::Factor (const Factor& g)
{
  clone (g);
}



Factor::Factor (
    const VarIds& vids,
    const Ranges& ranges,
    const Params& params,
    unsigned distId)
{
  args_   = vids;
  ranges_ = ranges;
  params_ = params;
  distId_ = distId;
  assert (params_.size() == Util::sizeExpected (ranges_));
}



Factor::Factor (
    const Vars& vars,
    const Params& params,
    unsigned distId)
{
  for (size_t i = 0; i < vars.size(); i++) {
    args_.push_back (vars[i]->varId());
    ranges_.push_back (vars[i]->range());
  }
  params_ = params;
  distId_ = distId;
  assert (params_.size() == Util::sizeExpected (ranges_));
}



void
Factor::sumOut (VarId vid)
{
  if (vid == args_.front() && ranges_.front() == 2) {
    // optimization
    sumOutFirstVariable();
  } else if (vid == args_.back() && ranges_.back() == 2) {
    // optimization
    sumOutLastVariable();
  } else {
    assert (indexOf (vid) != args_.size());
    sumOutIndex (indexOf (vid));
  }
}



void
Factor::sumOutAllExcept (VarId vid)
{
  assert (indexOf (vid) != args_.size());
  sumOutAllExceptIndex (indexOf (vid));
}



void
Factor::sumOutAllExcept (const VarIds& vids)
{
  vector<bool> mask (args_.size(), false);
  for (unsigned i = 0; i < vids.size(); i++) {
    assert (indexOf (vids[i]) != args_.size());
    mask[indexOf (vids[i])] = true;
  }
  sumOutArgs (mask);
}



void
Factor::sumOutAllExceptIndex (size_t idx)
{
  assert (idx < args_.size());
  vector<bool> mask (args_.size(), false);
  mask[idx] = true;
  sumOutArgs (mask);
}


void
Factor::multiply (Factor& g)
{
  if (args_.size() == 0) {
    clone (g);
    return;
  }
  TFactor<VarId>::multiply (g);
}



string
Factor::getLabel (void) const
{
  stringstream ss;
  ss << "f(" ;
  for (size_t i = 0; i < args_.size(); i++) {
    if (i != 0) ss << "," ;
    ss << Var (args_[i], ranges_[i]).label();
  }
  ss << ")" ;
  return ss.str();
}



void
Factor::print (void) const
{
  Vars vars;
  for (size_t i = 0; i < args_.size(); i++) {
    vars.push_back (new Var (args_[i], ranges_[i]));
  }
  vector<string> jointStrings = Util::getStateLines (vars);
  for (size_t i = 0; i < params_.size(); i++) {
    // cout << "[" << distId_ << "] " ;
    cout << "f(" << jointStrings[i] << ")" ;
    cout << " = " << params_[i] << endl;
  }
  cout << endl;
  for (size_t i = 0; i < vars.size(); i++) {
    delete vars[i];
  }
}



void
Factor::sumOutFirstVariable (void)
{
  size_t sep = params_.size() / 2;
  if (Globals::logDomain) {
    std::transform (
        params_.begin(), params_.begin() + sep,
        params_.begin() + sep, params_.begin(),
        Util::logSum);

  } else {
    std::transform (
        params_.begin(), params_.begin() + sep,
        params_.begin() + sep, params_.begin(),
        std::plus<double>());
  }
  params_.resize (sep);
  args_.erase (args_.begin());
  ranges_.erase (ranges_.begin());
}



void
Factor::sumOutLastVariable (void)
{
  Params::iterator first1 = params_.begin();
  Params::iterator first2 = params_.begin();
  Params::iterator last   = params_.end();
  if (Globals::logDomain) {
    while (first2 != last) {
      // the arguments can be swaped, but that is ok
      *first1++ = Util::logSum (*first2++, *first2++);
    }
  } else {
    while (first2 != last) {
      *first1++ = (*first2++) + (*first2++);
    }
  }
  params_.resize (params_.size() / 2);
  args_.pop_back();
  ranges_.pop_back();
}



void
Factor::sumOutArgs (const vector<bool>& mask)
{
  assert (mask.size() == args_.size());
  size_t new_size = 1;
  Ranges oldRanges = ranges_;
  args_.clear();
  ranges_.clear();
  for (unsigned i = 0; i < mask.size(); i++) {
    if (mask[i]) {
      new_size *= ranges_[i];
      args_.push_back (args_[i]);
      ranges_.push_back (ranges_[i]);
    }
  }
  Params newps (new_size, LogAware::addIdenty());
  Params::const_iterator first = params_.begin();
  Params::const_iterator last  = params_.end();
  MapIndexer indexer (oldRanges, mask);
  if (Globals::logDomain) {
    while (first != last) {
      newps[indexer] = Util::logSum (newps[indexer], *first++);
      ++ indexer;
    }
  } else {
    while (first != last) {
      newps[indexer] += *first++;
      ++ indexer;
    }
  }
  params_ = newps;
}



void
Factor::clone (const Factor& g)
{
  args_    = g.arguments();
  ranges_  = g.ranges();
  params_  = g.params();
  distId_  = g.distId();
}

