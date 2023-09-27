#include "mutate.h"
#include <list>
#include <map>
#include <stack>
#include <unordered_set>

using namespace tree_guide;

namespace mutator {

/////////////////////////////////////////////////////////////////////////////////////
// TODO
// - lots more work on scope-aware mutations

enum class MutationKind { NUMBER, SCOPE_DELETE, SCOPE_SWAP, SCOPE_CLONE, MAX_MUTATION_KIND };

static std::unique_ptr<std::mt19937_64> Rand;

void init(long Seed) { Rand = std::make_unique<std::mt19937_64>(Seed); }

static void change_one_number(std::vector<rec> &C) {
  std::uniform_int_distribution<uint64_t> FullDist(
      std::numeric_limits<uint64_t>::min(),
      std::numeric_limits<uint64_t>::max());
  std::uniform_int_distribution<uint64_t> LimitedDist(0, C.size() - 1);
  uint64_t x;
  // this could perform poorly if a choice sequence is almost entirely
  // scope changes, or loop infinitely if there aren't any numeric
  // choices, we'll deal with that when it happens
  do {
    x = LimitedDist(*Rand);
  } while (C.at(x).k != RecKind::NUM);
  C.at(x).v = FullDist(*Rand);
}

static std::vector<std::pair<size_t, size_t>> find_scopes(const std::vector<rec> &C) {
  std::vector<std::pair<size_t, size_t>> scopes;
  std::stack<size_t> stack;
  for (size_t i = 0; i < C.size(); ++i) {
    if (C.at(i).k == RecKind::START) {
      stack.push(i);
    } else if (C.at(i).k == RecKind::END) {
      scopes.emplace_back(stack.top(), i);
      stack.pop();
    }
  }
  return scopes;
}

static void delete_scope(std::vector<rec> &C) {
  auto S = find_scopes(C);
  std::uniform_int_distribution<uint64_t> ScopesDist(0, S.size() - 1);
  auto SID = ScopesDist(*Rand);
  C.erase(C.begin() + S.at(SID).first, C.begin() + S.at(SID).second + 1);
}

// Auxiliary function that fills the map with depth to scope start and end
// Returns a set of depths that have two or more scopes
static std::unordered_set<size_t> find_depth_to_scope_map(std::vector<rec> &C,
                                                          std::unordered_map<size_t, std::vector<std::pair<size_t, size_t>>> &ret) {
  // Start pos
  std::stack<size_t> stack;
  // Current depth
  size_t depth = 0;
  // Possible depth that has two or more scopes
  std::unordered_set<size_t> depth_candidates;
  for (size_t i = 0; i < C.size(); ++i) {
    if (C.at(i).k == RecKind::START) {
      stack.emplace(i);
      depth++;
    } else if (C.at(i).k == RecKind::END) {
      depth--;
      auto s = stack.top();
      stack.pop();
      ret[depth].emplace_back(s, i);
      if (ret[depth].size() >= 2)
        depth_candidates.insert(depth);
    }
  }
  return depth_candidates;
}

static std::vector<std::pair<size_t, size_t>> chose_scopes(std::unordered_set<size_t> &depth_candidates,
                                                           std::unordered_map<size_t, std::vector<std::pair<size_t, size_t>>> &map) {
  // Choose at what depth do we want to swap scopes
  std::vector<size_t> depth_candidates_vec(depth_candidates.begin(), depth_candidates.end());
  std::uniform_int_distribution<uint64_t> DepthDist(0, depth_candidates_vec.size() - 1);
  auto DID = DepthDist(*Rand);
  auto D = depth_candidates_vec.at(DID);

  // Choose two scopes at the same depth
  std::vector<std::pair<size_t, size_t>> chosen_scopes;
  chosen_scopes.reserve(2);
  std::sample(map.at(D).begin(), map.at(D).end(), std::back_inserter(chosen_scopes), 2,*Rand);
  return chosen_scopes;
}

static void swap_scopes(std::vector<rec> &C) {
  // TODO: add a variant where we swap only neighboring scopes at the same level
  // Mapping of depth to scope start and end
  // We need to keep track of depth to scope correspondence because
  // we can only swap scopes at the same depth
  //TODO: can we relax this constraint?
  std::unordered_map<size_t, std::vector<std::pair<size_t, size_t>>> map;
  auto depth_candidates = find_depth_to_scope_map(C, map);
  if (depth_candidates.empty())
    return;

  auto chosen_scopes = chose_scopes(depth_candidates, map);
  auto SL = chosen_scopes.front();
  auto SR = chosen_scopes.back();

  if (SL.first > SR.first)
    std::swap(SL, SR);
  assert(SL.second < SR.first && "Scopes should not overlap");
  std::list<rec> prefix(C.begin(), C.begin() + SL.first);
  std::list<rec> sl(C.begin() + SL.first, C.begin() + SL.second + 1);
  std::list<rec> mid(C.begin() + SL.second + 1, C.begin() + SR.first);
  std::list<rec> sr(C.begin() + SR.first, C.begin() + SR.second + 1);
  std::list<rec> suffix(C.begin() + SR.second + 1, C.end());
  C.clear();
  C.insert(C.end(), prefix.begin(), prefix.end());
  C.insert(C.end(), sr.begin(), sr.end());
  C.insert(C.end(), mid.begin(), mid.end());
  C.insert(C.end(), sl.begin(), sl.end());
  C.insert(C.end(), suffix.begin(), suffix.end());
}

static void clone_scope(std::vector<rec> &C) {
  //TODO: do we want to clone the scope more than once?
  // Mapping of depth to scope start and end
  // We need to keep track of depth to scope correspondence because
  // we can only swap scopes at the same depth
  //TODO: can we relax this constraint?
  std::unordered_map<size_t, std::vector<std::pair<size_t, size_t>>> map;
  auto depth_candidates = find_depth_to_scope_map(C, map);
  if (depth_candidates.empty())
    return;

  auto chosen_scopes = chose_scopes(depth_candidates, map);
  auto SL = chosen_scopes.front();
  auto SR = chosen_scopes.back();

  if (SL.first > SR.first)
    std::swap(SL, SR);
  assert(SL.second < SR.first && "Scopes should not overlap");
  // We copy the first scope to the place of the second scope
  // We assume that the opposite case will be handled by the swap mutation
  std::list<rec> prefix(C.begin(), C.begin() + SL.first);
  std::list<rec> s(C.begin() + SL.first, C.begin() + SL.second + 1);
  std::list<rec> mid(C.begin() + SL.second + 1, C.begin() + SR.first);
  std::list<rec> suffix(C.begin() + SR.second + 1, C.end());
  C.clear();
  C.insert(C.end(), prefix.begin(), prefix.end());
  C.insert(C.end(), s.begin(), s.end());
  C.insert(C.end(), mid.begin(), mid.end());
  C.insert(C.end(), s.begin(), s.end());
  C.insert(C.end(), suffix.begin(), suffix.end());
}

void mutate_choices(std::vector<rec> &C) {
  std::uniform_int_distribution<uint64_t> CoinDist(0, 1);
  std::uniform_int_distribution<uint64_t> MutationKindDist(
      0, static_cast<uint64_t>(MutationKind::MAX_MUTATION_KIND) - 1);
  do {
    auto MID = static_cast<MutationKind>(MutationKindDist(*Rand.get()));
    switch (MID) {
    case MutationKind::NUMBER:
      change_one_number(C);
      break;
    case MutationKind::SCOPE_DELETE:
      delete_scope(C);
      break;
    case MutationKind::SCOPE_SWAP:
      swap_scopes(C);
      break;
    case MutationKind::SCOPE_CLONE:
      clone_scope(C);
      break;
    case MutationKind::MAX_MUTATION_KIND:
      assert(false && "MAX_MUTATION_KIND is not a valid mutation kind");
      break;
    }
  } while (CoinDist(*Rand) == 0);
}

} // end namespace mutator

