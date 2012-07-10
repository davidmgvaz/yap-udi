#include "CountingBp.h"
#include "WeightedBp.h"


bool CountingBp::checkForIdenticalFactors = true;


CountingBp::CountingBp (const FactorGraph& fg)
    : Solver (fg), freeColor_(0)
{
  findIdenticalFactors();
  setInitialColors();
  createGroups();
  compressedFg_ = getCompressedFactorGraph();
  solver_ = new WeightedBp (*compressedFg_, getWeights());
}



CountingBp::~CountingBp (void)
{
  delete solver_;
  delete compressedFg_;
  for (size_t i = 0; i  < varClusters_.size(); i++) {
    delete varClusters_[i];
  }
  for (size_t i = 0; i  < facClusters_.size(); i++) {
    delete facClusters_[i];
  }
}



void
CountingBp::printSolverFlags (void) const
{
  stringstream ss;
  ss << "counting bp [" ;
  ss << "schedule=" ;
  typedef BpOptions::Schedule Sch;
  switch (BpOptions::schedule) {
    case Sch::SEQ_FIXED:    ss << "seq_fixed";    break;
    case Sch::SEQ_RANDOM:   ss << "seq_random";   break;
    case Sch::PARALLEL:     ss << "parallel";     break;
    case Sch::MAX_RESIDUAL: ss << "max_residual"; break;
  }
  ss << ",max_iter=" << BpOptions::maxIter;
  ss << ",accuracy=" << BpOptions::accuracy;
  ss << ",log_domain=" << Util::toString (Globals::logDomain);
  ss << ",chkif=" << 
      Util::toString (CountingBp::checkForIdenticalFactors);
  ss << "]" ;
  cout << ss.str() << endl;
}



Params
CountingBp::solveQuery (VarIds queryVids)
{
  assert (queryVids.empty() == false);
  Params res;
  if (queryVids.size() == 1) {
    res = solver_->getPosterioriOf (getRepresentative (queryVids[0]));
  } else {
    VarNode* vn = fg.getVarNode (queryVids[0]);
    const FacNodes& facNodes = vn->neighbors();
    size_t idx = facNodes.size();
    for (size_t i = 0; i < facNodes.size(); i++) {
      if (facNodes[i]->factor().contains (queryVids)) {
        idx = i;
        break;
      }
      cout << endl;
    }
    if (idx == facNodes.size()) {
      res = Solver::getJointByConditioning (
          GroundSolver::CBP, fg, queryVids);
    } else {
      VarIds reprArgs;
      for (size_t i = 0; i < queryVids.size(); i++) {
        reprArgs.push_back (getRepresentative (queryVids[i]));
      }
      res = solver_->getFactorJoint (idx, reprArgs);
    }
  }
  return res;
}



void
CountingBp::findIdenticalFactors()
{
  const FacNodes& facNodes = fg.facNodes();
  if (checkForIdenticalFactors == false ||
      facNodes.size() == 1) {
    return;
  }
  for (size_t i = 0; i < facNodes.size(); i++) {
    facNodes[i]->factor().setDistId (Util::maxUnsigned());
  }
  unsigned groupCount = 1;
  for (size_t i = 0; i < facNodes.size() - 1; i++) {
    Factor& f1 = facNodes[i]->factor();
    if (f1.distId() != Util::maxUnsigned()) {
      continue;
    }
    f1.setDistId (groupCount);
    for (size_t j = i + 1; j < facNodes.size(); j++) {
      Factor& f2 = facNodes[j]->factor();
      if (f2.distId() != Util::maxUnsigned()) {
        continue;
      }
      if (f1.size()   == f2.size()   &&
          f1.ranges() == f2.ranges() &&
          f1.params() == f2.params()) {
        f2.setDistId (groupCount);
      }
    }
    groupCount ++;
  }
}



void
CountingBp::setInitialColors (void)
{
  varColors_.resize (fg.nrVarNodes());
  facColors_.resize (fg.nrFacNodes());
  // create the initial variable colors
  VarColorMap colorMap;
  const VarNodes& varNodes = fg.varNodes();
  for (size_t i = 0; i < varNodes.size(); i++) {
    unsigned range = varNodes[i]->range();
    VarColorMap::iterator it = colorMap.find (range);
    if (it == colorMap.end()) {
      it = colorMap.insert (make_pair (
          range, Colors (range + 1, -1))).first; 
    }
    unsigned idx = varNodes[i]->hasEvidence()
                 ? varNodes[i]->getEvidence()
                 : range;
    Colors& stateColors = it->second;
    if (stateColors[idx] == -1) {
      stateColors[idx] = getNewColor();
    }
    setColor (varNodes[i], stateColors[idx]);
  }
  const FacNodes& facNodes = fg.facNodes();
  // create the initial factor colors
  DistColorMap distColors;
  for (size_t i = 0; i < facNodes.size(); i++) {
    unsigned distId = facNodes[i]->factor().distId();
    DistColorMap::iterator it = distColors.find (distId);
    if (it == distColors.end()) {
      it = distColors.insert (make_pair (distId, getNewColor())).first;
    }
    setColor (facNodes[i], it->second);
  }
}



void
CountingBp::createGroups (void)
{
  VarSignMap varGroups;
  FacSignMap facGroups;
  unsigned nIters = 0;
  bool groupsHaveChanged = true;
  const VarNodes& varNodes = fg.varNodes();
  const FacNodes& facNodes = fg.facNodes();

  while (groupsHaveChanged || nIters == 1) {
    nIters ++;

    // set a new color to the variables with the same signature
    size_t prevVarGroupsSize = varGroups.size();
    varGroups.clear();
    for (size_t i = 0; i < varNodes.size(); i++) {
      const VarSignature& signature = getSignature (varNodes[i]);
      VarSignMap::iterator it = varGroups.find (signature);
      if (it == varGroups.end()) {
        it = varGroups.insert (make_pair (signature, VarNodes())).first;
      }
      it->second.push_back (varNodes[i]);
    }
    for (VarSignMap::iterator it = varGroups.begin();
        it != varGroups.end(); ++it) {
      Color newColor = getNewColor();
      VarNodes& groupMembers = it->second;
      for (size_t i = 0; i < groupMembers.size(); i++) {
        setColor (groupMembers[i], newColor);
      }
    }

    size_t prevFactorGroupsSize = facGroups.size();
    facGroups.clear();
    // set a new color to the factors with the same signature
    for (size_t i = 0; i < facNodes.size(); i++) {
      const FacSignature& signature = getSignature (facNodes[i]);
      FacSignMap::iterator it = facGroups.find (signature);
      if (it == facGroups.end()) {
        it = facGroups.insert (make_pair (signature, FacNodes())).first;
      }
      it->second.push_back (facNodes[i]);
    }
    for (FacSignMap::iterator it = facGroups.begin();
        it != facGroups.end(); ++it) {
      Color newColor = getNewColor();
      FacNodes& groupMembers = it->second;
      for (size_t i = 0; i < groupMembers.size(); i++) {
        setColor (groupMembers[i], newColor);
      }
    }

    groupsHaveChanged = prevVarGroupsSize != varGroups.size()
        || prevFactorGroupsSize != facGroups.size();
  }
  // printGroups (varGroups, facGroups);
  createClusters (varGroups, facGroups);
}



void
CountingBp::createClusters (
    const VarSignMap& varGroups,
    const FacSignMap& facGroups)
{
  varClusters_.reserve (varGroups.size());
  for (VarSignMap::const_iterator it = varGroups.begin();
       it != varGroups.end(); ++it) {
    const VarNodes& groupVars = it->second;
    VarCluster* vc = new VarCluster (groupVars);
    for (size_t i = 0; i < groupVars.size(); i++) {
      varClusterMap_.insert (make_pair (groupVars[i]->varId(), vc));
    }
    varClusters_.push_back (vc);
  }

  facClusters_.reserve (facGroups.size());
  for (FacSignMap::const_iterator it = facGroups.begin();
       it != facGroups.end(); ++it) {
    FacNode* groupFactor = it->second[0];
    const VarNodes& neighs = groupFactor->neighbors();
    VarClusters varClusters;
    varClusters.reserve (neighs.size());
    for (size_t i = 0; i < neighs.size(); i++) {
      VarId vid = neighs[i]->varId();
      varClusters.push_back (varClusterMap_.find (vid)->second);
    }
    facClusters_.push_back (new FacCluster (it->second, varClusters));
  }
}



VarSignature
CountingBp::getSignature (const VarNode* varNode)
{
  const FacNodes& neighs = varNode->neighbors();
  VarSignature sign;
  sign.reserve (neighs.size() + 1);
  for (size_t i = 0; i < neighs.size(); i++) {
    sign.push_back (make_pair (
        getColor (neighs[i]),
        neighs[i]->factor().indexOf (varNode->varId())));
  }
  std::sort (sign.begin(), sign.end());
  sign.push_back (make_pair (getColor (varNode), 0));
  return sign;
}



FacSignature
CountingBp::getSignature (const FacNode* facNode)
{
  const VarNodes& neighs = facNode->neighbors();
  FacSignature sign;
  sign.reserve (neighs.size() + 1);
  for (size_t i = 0; i < neighs.size(); i++) {
    sign.push_back (getColor (neighs[i]));
  }
  sign.push_back (getColor (facNode));
  return sign;
}



VarId
CountingBp::getRepresentative (VarId vid)
{
  assert (Util::contains (varClusterMap_, vid));
  VarCluster* vc = varClusterMap_.find (vid)->second;
  return vc->representative()->varId();
}



FacNode*
CountingBp::getRepresentative (FacNode* fn)
{
  for (size_t i = 0; i < facClusters_.size(); i++) {
    if (Util::contains (facClusters_[i]->members(), fn)) {
      return facClusters_[i]->representative();
    }
  }
  return 0;
}



FactorGraph*
CountingBp::getCompressedFactorGraph (void)
{
  FactorGraph* fg = new FactorGraph();
  for (size_t i = 0; i < varClusters_.size(); i++) {
    VarNode* newVar = new VarNode (varClusters_[i]->first());
    varClusters_[i]->setRepresentative (newVar);
    fg->addVarNode (newVar);
  }
  for (size_t i = 0; i < facClusters_.size(); i++) {
    Vars vars;
    const VarClusters& clusters = facClusters_[i]->varClusters();
    for (size_t j = 0; j < clusters.size(); j++) {
      vars.push_back (clusters[j]->representative());
    }
    const Factor& groundFac = facClusters_[i]->first()->factor();
    FacNode* fn = new FacNode (Factor (
        vars, groundFac.params(), groundFac.distId()));
    facClusters_[i]->setRepresentative (fn);
    fg->addFacNode (fn);
    for (size_t j = 0; j < vars.size(); j++) {
      fg->addEdge (static_cast<VarNode*> (vars[j]), fn);
    }
  }
  return fg;
}



vector<vector<unsigned>>
CountingBp::getWeights (void) const
{
  vector<vector<unsigned>> weights;
  weights.reserve (facClusters_.size());
  for (size_t i = 0; i < facClusters_.size(); i++) {
    const VarClusters& neighs = facClusters_[i]->varClusters();
    weights.push_back ({ });
    weights.back().reserve (neighs.size());
    for (size_t j = 0; j < neighs.size(); j++) {
      weights.back().push_back (getWeight (
          facClusters_[i], neighs[j], j));
    }
  }
  return weights;
}



unsigned
CountingBp::getWeight (
    const FacCluster* fc,
    const VarCluster* vc,
    size_t index) const
{
  unsigned weight = 0;
  VarId reprVid = vc->representative()->varId();
  VarNode* groundVar = fg.getVarNode (reprVid);
  const FacNodes& neighs = groundVar->neighbors();
  for (size_t i = 0; i < neighs.size(); i++) {
    FacNodes::const_iterator it;
    it = std::find (fc->members().begin(), fc->members().end(), neighs[i]);
    if (it != fc->members().end() &&
        (*it)->factor().indexOf (reprVid) == index) {
      weight ++;
    }
  }
  return weight;
}



void
CountingBp::printGroups (
    const VarSignMap& varGroups,
    const FacSignMap& facGroups) const
{
  unsigned count = 1;
  cout << "variable groups:" << endl;
  for (VarSignMap::const_iterator it = varGroups.begin();
      it != varGroups.end(); ++it) {
    const VarNodes& groupMembers = it->second;
    if (groupMembers.size() > 0) {
      cout << count << ": " ;
      for (size_t i = 0; i < groupMembers.size(); i++) {
        cout << groupMembers[i]->label() << " " ;
      }
      count ++;
      cout << endl;
    }
  }
  count = 1;
  cout << endl << "factor groups:" << endl;
  for (FacSignMap::const_iterator it = facGroups.begin();
      it != facGroups.end(); ++it) {
    const FacNodes& groupMembers = it->second;
    if (groupMembers.size() > 0) {
      cout << ++count << ": " ;
      for (size_t i = 0; i < groupMembers.size(); i++) {
        cout << groupMembers[i]->getLabel() << " " ;
      }
      count ++;
      cout << endl;
    }
  }
}

