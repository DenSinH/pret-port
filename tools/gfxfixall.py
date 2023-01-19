from pymakeparser.pymakeparser import Makefile
import sys


if __name__ == '__main__':
    _, gbagfx, *makefiles = sys.argv

    print(gbagfx)
    print(makefiles)

