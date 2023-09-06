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

//enum class MutationKind { NUMBER, SCOPE_DELETE, MAX_MUTATION_KIND, SCOPE_SWAP, SCOPE_COPY, SCOPE_ADD,  };
enum class MutationKind { SCOPE_SWAP, MAX_MUTATION_KIND };

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

static void swap_scopes(std::vector<rec> &C) {
  // TODO: add a variant where we swap only neighboring scopes at the same level
  // Start pos
  std::stack<size_t> stack;
  // Mapping for depth to scope
  std::unordered_map<size_t, std::vector<std::pair<size_t, size_t>>> map;
  size_t depth = 0;
  std::unordered_set<size_t> depth_candidates;
  for (size_t i = 0; i < C.size(); ++i) {
    if (C.at(i).k == RecKind::START) {
      stack.emplace(i);
      depth++;
    } else if (C.at(i).k == RecKind::END) {
      depth--;
      auto s = stack.top();
      stack.pop();
      map[depth].emplace_back(s, i);
      if (map[depth].size() >= 2)
        depth_candidates.insert(depth);
    }
  }
  std::vector<size_t> depth_candidates_vec(depth_candidates.begin(), depth_candidates.end());
  std::uniform_int_distribution<uint64_t> DepthDist(0, depth_candidates_vec.size() - 1);
  auto DID = DepthDist(*Rand);
  auto D = depth_candidates_vec.at(DID);

  for (const auto &scope : map.at(D))
    std::cout << scope.first << " " << scope.second << "\n";

  std::vector<std::pair<size_t, size_t>> chosen_scopes;
  chosen_scopes.reserve(2);
  std::sample(map.at(D).begin(), map.at(D).end(), std::back_inserter(chosen_scopes), 2,*Rand);

  auto S_l = chosen_scopes.front();
  auto S_r = chosen_scopes.back();

  std::cout << "S_l: " << S_l.first << " " << S_l.second << "\n";
  std::cout << "S_r: " << S_r.first << " " << S_r.second << "\n";

  if (S_l.first > S_r.first)
    std::swap(S_l, S_r);
  assert(S_l.second < S_r.first && "Scopes should not overlap");
  std::list<rec> left(C.begin(), C.begin() + S_l.first);
  std::list<rec> s_l_c(C.begin() + S_l.first, C.begin() + S_l.second + 1);
  std::list<rec> mid(C.begin() + S_l.second + 1, C.begin() + S_r.first);
  std::list<rec> s_r_c(C.begin() + S_r.first, C.begin() + S_r.second + 1);
  std::list<rec> right(C.begin() + S_r.second + 1, C.end());
  C.clear();
  C.insert(C.end(), left.begin(), left.end());
  C.insert(C.end(), s_r_c.begin(), s_r_c.end());
  C.insert(C.end(), mid.begin(), mid.end());
  C.insert(C.end(), s_l_c.begin(), s_l_c.end());
  C.insert(C.end(), right.begin(), right.end());
}

void mutate_choices(std::vector<rec> &C) {
  std::uniform_int_distribution<uint64_t> CoinDist(0, 1);
  std::uniform_int_distribution<uint64_t> MutationKindDist(
      0, static_cast<uint64_t>(MutationKind::MAX_MUTATION_KIND) - 1);
  do {
    switch (static_cast<MutationKind>(MutationKindDist(*Rand.get()))) {
      /*
      case MutationKind::NUMBER:
        change_one_number(C);
        break;
      case MutationKind::SCOPE_DELETE:
        delete_scope(C);
        break;
      */
    case MutationKind::SCOPE_SWAP:
      swap_scopes(C);
      break;
    case MutationKind::MAX_MUTATION_KIND:
      assert(false && "MAX_MUTATION_KIND is not a valid mutation kind");
      break;
    }
  } while (/*CoinDist(*Rand) == 0*/ false);
}

} // end namespace mutator

