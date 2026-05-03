#include "utils.hpp"
#include <sstream>

using namespace std;

vector<string> split(const string &texto, char delimitador) {
  vector<string> partes;
  stringstream ss(texto);
  string item;

  while (getline(ss, item, delimitador)) {
    partes.push_back(item);
  }

  return partes;
}
