#include "gen_tree.h"
#include "mutate.h"
std::string Prefix, Generator;

size_t tree_generator::TreeNode::GID = 0;

static std::string getEnvVar(std::string const &var) {
  char const *val = getenv(var.c_str());
  return (val == nullptr) ? std::string() : std::string(val);
}

static std::shared_ptr<tree_generator::TreeNode> generateNodeImpl(tree_guide::Chooser &C, size_t cur_depth) {
  C.beginScope();
  // The 100 offset is required to identify the data choice in the choice sequence.
  std::string data = std::to_string(100 + C.choose(10));

  C.beginScope();
  size_t num_children = C.choose(tree_generator::MAX_CHILDREN);
  if (cur_depth == tree_generator::MAX_DEPTH)
    num_children = 0;
  std::vector<std::shared_ptr<tree_generator::TreeNode>> children;
  children.reserve(num_children);
  for (size_t i = 0; i < num_children; ++i) {
    children.emplace_back(generateNodeImpl(C, cur_depth + 1));
  }
  C.endScope();
  C.endScope();
  return std::make_shared<tree_generator::TreeNode>(data, children);
}

std::shared_ptr<tree_generator::TreeNode> tree_generator::TreeNode::generateNode(tree_guide::Chooser &C) {
  auto N = generateNodeImpl(C, 0);
  return N;
}

void tree_generator::TreeNode::dumpDOT(std::ostream &O) const {
  O << id << " [label=\"" << _data << "\"];\n";
  for (auto &C : _children) {
    C->dumpDOT(O);
    O << id << " -- " << C->id << ";\n";
  }
}

int main(int argc, char *argv []) {
  if (argc < 5) {
    std::cerr << "Usage: " << argv[0]
              << "seed output_sequence output_dot_file mode(load, mutate, or gen) input_sequence\n";
    return 1;
  }

  std::string seed_str = argv[1];
  size_t seed = std::stoull(seed_str);
  if (seed == 0)
    seed = std::random_device{}();
  std::cout << "Seed: " << seed << "\n";
  std::string output_sequence = argv[2];
  std::string output_dot_file = argv[3];
  std::string mode = argv[4];
  if (mode != "load" && mode != "mutate" && mode != "gen") {
    std::cerr << "Invalid mode: " << mode << "\n";
    return 1;
  }

  std::string input_sequence;
  if (mode == "load" || mode == "mutate") {
    if (argc < 6) {
      std::cerr << argv[0]
                << " requires an input sequence file when mode is load or mutate\n";
      return 1;
    }
    input_sequence = argv[5];
  }

  std::string Prefix = "//";
  tree_guide::Guide *G;
  if (mode == "gen") {
    G = new tree_guide::DefaultGuide(seed);
  }
  else {
    auto FG = new tree_guide::FileGuide();
    FG->parseChoices(input_sequence, Prefix);
    G = FG;
    if (mode == "mutate") {
      mutator::init(seed);
      FG->setSync(tree_guide::Sync::RESYNC);
      auto C1 = FG->getChoices();
      mutator::mutate_choices(C1);
      FG->replaceChoices(C1);
    }
  }

  auto SG = tree_guide::SaverGuide(G, Prefix);
  auto C = SG.makeChooser();
  auto N = tree_generator::TreeNode::generateNode(*C);
  std::ofstream output_sequence_file(output_sequence);
  output_sequence_file
      << static_cast<tree_guide::SaverChooser *>(C.get())->formatChoices()
      << "\n";
  output_sequence_file.close();

  std::ofstream output_dot_stream(output_dot_file);
  output_dot_stream << "graph {\n";
  N->dumpDOT(output_dot_stream);
  output_dot_stream << "}\n";

  return 0;
}