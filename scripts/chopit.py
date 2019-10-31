#!/usr/bin/env python3
from diffanalyze import OutputManager,RepoManager

import argparse
import os
import sys

# check colour support
try:
    from termcolor import colored
except ImportError:
    hasColourSupport = False
else:
    hasColourSupport = sys.stdout.isatty()
if not hasColourSupport:
    def colored(text, color):
        return text

##### Main program #####
def main(main_args):
    # Initialize argparse
    parser = argparse.ArgumentParser(
        description='Chopper+diffanalyze+dot toolchain')

    parser.add_argument('file', metavar='bcfile', help='.bc bytecode file to analyze')
    parser.add_argument('-f', '--noskip', help='Functions *not* to skip')
    parser.add_argument('-r', '--repo', help='Repository to get the diff from')
    parser.add_argument('-w', '--wfilter', help='Warnings to filter')
    # from diffanalyze
    parser.add_argument('--revision', help='repository revision', default='HEAD')
    # parser.add_argument('-ri', '--rangeInt', type=int, metavar='N', help='look at patches for the previous N commits (preceding HASH)')
    parser.add_argument('-rh', '--range', metavar='INIT_HASH', help='look at patches between INIT_HASH and HASH', default='HEAD~1')

    # Dictionary of arguments
    args_orig = parser.parse_args(main_args)
    args = vars(args_orig)

    # Get functions to (not) skip
    noskip = ''
    if args['noskip']:
        print("Using function list method")
        noskip = args['noskip']

    elif args['repo']:
        #TODO: Need to modify main_args to include repo
        #diffanalyze.main(main_args)

        print("Using diffanalyze method")
        print("Getting repository: \033[0;36m" + args['repo'] + "\033[0m...", end='', flush=True)
        repo_manager = RepoManager(args['repo'], 'only-fn', 'false', 'diff', '')
        print("\033[1;32m OK\033[0m")

        print("Scanning commits from \033[0;36m" + args['revision'] + "\033[0m to \033[0;36m" + args['range'] + "\033[0m for function list... [", flush=True)
        diff_summary_list = repo_manager.compare_patches_in_range(args['revision'],args['range'])
        print("]\033[1;32m OK\033[0m")

        for diff_summary in diff_summary_list:
            for diff_data in diff_summary.file_diffs:
                for fn_name, lines in diff_data.fn_to_changed_lines.items():
                    if lines:
                        if noskip != '':
                            noskip += ','
                        noskip += fn_name
    else:
        print('Error: you must provide either --noskip or --repo')
        return
    print("No-skip functions list: " + colored(noskip, 'green'))

    # Get callgraph
    print ("Generating callgraph.dot...")
    os.system("opt -dot-callgraph " + args['file'] + " 1>/dev/null")
    print ("...\033[1;32m OK\033[0m")
    print ("Generating callgraph-chopped.dot...", end='', flush=True)
    os.system("choppy-dot " + noskip)
    print ("\033[1;32m OK\033[0m")

    # CHOPPER
    wfilter = ""
    if args['wfilter']:
        wfilter = "-w=" + args['wfilter'] + " "
    klee_command="klee -libc=uclibc -simplify-sym-indices -search=nurs:covnew -split-search -output-module -skip-functions-not=" + noskip + " " + wfilter + args['file']
    print("Running Chopper...\n" + colored(klee_command, 'yellow'))

    os.system(klee_command)

if __name__ == '__main__':
    main(sys.argv[1:])