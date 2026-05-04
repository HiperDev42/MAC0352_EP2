#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace mib {

struct Node {
  std::string label;
  std::function<std::string()> getter; // if set, this is a leaf
  std::map<int, std::unique_ptr<Node>> children;
};

class Tree {
public:
  Tree();

  void register_oid(const std::vector<int> &path, const std::string &label,
                    std::function<std::string()> getter = nullptr);
  std::string handle_get(const std::string &oid_str) const;

private:
  Node root;

  const Node *find_node(const std::vector<int> &path) const;
  void collect_leaves(const Node *node, const std::vector<int> &prefix,
                      std::vector<std::pair<std::string, std::string>> &out) const;
  static std::vector<int> parse_oid(const std::string &oid_str);
  static std::string oid_to_string(const std::vector<int> &path);
};

Tree &global_tree();

} // namespace mib
