#include "mib.hpp"

#include <iomanip>
#include <sstream>

using namespace std;

namespace mib {

Tree::Tree() {}

void Tree::register_oid(const vector<int> &path, const string &label,
                        function<string()> getter) {
  Node *cur = &root;
  for (int p : path) {
    if (!cur->children.count(p)) {
      cur->children[p] = make_unique<Node>();
    }
    cur = cur->children[p].get();
  }
  cur->label = label;
  cur->getter = getter;
}

vector<int> Tree::parse_oid(const string &oid_str) {
  vector<int> out;
  if (oid_str.empty() || oid_str == ".") return out;
  // expect format like .1.2
  size_t i = 0;
  while (i < oid_str.size()) {
    if (oid_str[i] == '.') {
      ++i;
      size_t j = i;
      while (j < oid_str.size() && isdigit((unsigned char)oid_str[j])) ++j;
      if (j > i) {
        int val = stoi(oid_str.substr(i, j - i));
        out.push_back(val);
        i = j;
      } else {
        break;
      }
    } else {
      break;
    }
  }
  return out;
}

const Node *Tree::find_node(const vector<int> &path) const {
  const Node *cur = &root;
  for (int p : path) {
    auto it = cur->children.find(p);
    if (it == cur->children.end()) return nullptr;
    cur = it->second.get();
  }
  return cur;
}

string Tree::oid_to_string(const vector<int> &path) {
  if (path.empty()) return string(".");
  ostringstream os;
  for (int i = 0; i < (int)path.size(); ++i) {
    os << '.' << path[i];
  }
  return os.str();
}

void Tree::collect_leaves(const Node *node, const vector<int> &prefix,
                          vector<pair<string, string>> &out) const {
  if (node->getter) {
    string oid = oid_to_string(prefix);
    string val = node->getter();
    out.emplace_back(oid, val);
  }

  for (auto &kv : node->children) {
    vector<int> next = prefix;
    next.push_back(kv.first);
    collect_leaves(kv.second.get(), next, out);
  }
}

string Tree::handle_get(const string &oid_str) const {
  vector<int> path = parse_oid(oid_str);
  const Node *node = find_node(path);
  if (!node) return string();

  vector<pair<string, string>> leaves;
  vector<int> prefix = path;
  collect_leaves(node, prefix, leaves);

  if (leaves.empty()) return string();

  ostringstream os;
  for (size_t i = 0; i < leaves.size(); ++i) {
    if (i) os << ';';
    os << leaves[i].first << '=' << leaves[i].second;
  }
  return os.str();
}

// Singleton global
Tree &global_tree() {
  static Tree t;
  return t;
}

} // namespace mib
