from graphviz import Source
import argparse

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('inp_file', default='tree.dot', help='Input DOT file for vizualization')
    args = parser.parse_args()

    s = Source.from_file(args.inp_file)
    s.view()
