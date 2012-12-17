#ifndef HORUS_COUNTINGBP_H
#define HORUS_COUNTINGBP_H

#include <unordered_map>

#include "GroundSolver.h"
#include "FactorGraph.h"
#include "Util.h"
#include "Horus.h"

class VarCluster;
class FacCluster;
class WeightedBp;

typedef long Color;
typedef vector<Color> Colors;
typedef vector<std::pair<Color,unsigned>> VarSignature;
typedef vector<Color> FacSignature;

typedef unordered_map<unsigned, Color>  DistColorMap;
typedef unordered_map<unsigned, Colors> VarColorMap;

typedef unordered_map<VarSignature, VarNodes> VarSignMap;
typedef unordered_map<FacSignature, FacNodes> FacSignMap;

typedef unordered_map<VarId, VarCluster*> VarClusterMap;

typedef vector<VarCluster*> VarClusters;
typedef vector<FacCluster*> FacClusters;

template <class T>
inline size_t hash_combine (size_t seed, const T& v)
{
  return seed ^ (hash<T>()(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2));
}


namespace std {
  template <typename T1, typename T2> struct hash<std::pair<T1,T2>>
  {
    size_t operator() (const std::pair<T1,T2>& p) const
    {
      return hash_combine (std::hash<T1>()(p.first), p.second);
    }
  };

  template <typename T> struct hash<std::vector<T>>
  {
    size_t operator() (const std::vector<T>& vec) const
    {
      size_t h = 0;
      typename vector<T>::const_iterator first = vec.begin();
      typename vector<T>::const_iterator last  = vec.end();
      for (; first != last; ++first) {
        h = hash_combine (h, *first);
      }
      return h;
    }
  };
}


class VarCluster
{
  public:
    VarCluster (const VarNodes& vs) : members_(vs) { }

    const VarNode* first (void) const { return members_.front(); }

    const VarNodes& members (void) const { return members_; }

    VarNode* representative (void) const { return repr_; }

    void setRepresentative (VarNode* vn) { repr_ = vn; }

  private:
    VarNodes  members_;
    VarNode*  repr_;
};


class FacCluster
{
  public:
    FacCluster (const FacNodes& fcs, const VarClusters& vcs)
        : members_(fcs), varClusters_(vcs) { }

    const FacNode* first (void) const { return members_.front(); }

    const FacNodes& members (void) const { return members_; }
   
    FacNode* representative (void) const { return repr_; }

    void setRepresentative (FacNode* fn) { repr_ = fn; }

    VarClusters& varClusters (void) { return varClusters_; }
 
  private:
    FacNodes     members_;
    FacNode*     repr_;
    VarClusters  varClusters_;
};


class CountingBp : public GroundSolver
{
  public:
    CountingBp (const FactorGraph& fg);

   ~CountingBp (void);

    void printSolverFlags (void) const;

    Params solveQuery (VarIds);
   
    static bool checkForIdenticalFactors;
 
  private:
    Color getNewColor (void)
    {
      ++ freeColor_;
      return freeColor_ - 1;
    }

    Color getColor (const VarNode* vn) const
    {
      return varColors_[vn->getIndex()];
    }

    Color getColor (const FacNode* fn) const
    {
      return facColors_[fn->getIndex()];
    }

    void setColor (const VarNode* vn, Color c)
    {
      varColors_[vn->getIndex()] = c;
    }

    void setColor (const FacNode* fn, Color  c)
    {
      facColors_[fn->getIndex()] = c;
    }

    void findIdenticalFactors (void);

    void setInitialColors (void);

    void createGroups (void);

    void createClusters (const VarSignMap&, const FacSignMap&);

    VarSignature getSignature (const VarNode*);

    FacSignature getSignature (const FacNode*);

    void printGroups (const VarSignMap&, const FacSignMap&) const;

    VarId getRepresentative (VarId vid);

    FacNode* getRepresentative (FacNode*);

    FactorGraph* getCompressedFactorGraph (void);

    vector<vector<unsigned>> getWeights (void) const;

    unsigned getWeight (const FacCluster*,
        const VarCluster*, size_t index) const;


    Color               freeColor_;
    Colors              varColors_;
    Colors              facColors_;
    VarClusters         varClusters_;
    FacClusters         facClusters_;
    VarClusterMap       varClusterMap_;
    const FactorGraph*  compressedFg_;
    WeightedBp*         solver_;
};

#endif // HORUS_COUNTINGBP_H

