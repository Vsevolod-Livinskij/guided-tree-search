#pragma once

#include "guide.h"

#include <memory>
#include <string>
#include <vector>

namespace tree_generator {

constexpr size_t MAX_CHILDREN = 3;
constexpr size_t MAX_DEPTH = 5;

struct TreeNode {
  TreeNode(std::string _data, std::vector<std::shared_ptr<TreeNode>> _children)
      : _data(std::move(_data)), _children(std::move(_children)), depth(0), id(GID++) {}
  void dumpDOT(std::ostream &O) const;
  static std::shared_ptr<TreeNode> generateNode(tree_guide::Chooser &C);

  std::string _data;
  std::vector<std::shared_ptr<TreeNode>> _children;
  size_t depth;
  size_t id;

  static size_t GID;
};

}
