#!/bin/python

import os
import subprocess
import sys
import pandas as pd
import numpy as np
import itertools
import coloredlogs
import logging
import argparse

parser = argparse.ArgumentParser()
parser.add_argument('--bin', help='Path to algorithm executable', type=str)
parser.add_argument('--data', help='Data path (can be either a directory or a .csv file', type=str)
parser.add_argument('--reps', help='The number of repetitions for each configuration', type=int)

args = parser.parse_args()

bin_path=args.bin
data_path=args.data
base_path = os.path.dirname(data_path)

reps = args.reps

population_size = [ 1000 ]
iteration_count = [ 0 ]
evaluation_budget = [ 500000 ]

meta_header = ['Problem', 
        'Pop size',
        'Iter count',
        'Eval count',
        'Run index']

output_header = ['Elapsed', 
        'Generation', 
        'Avg length', 
        'Diversity hybrid',
        'Diversity struct',
        'Avg quality', 
        'Sel pressure', 
        'Fitness evaluations', 
        'Local evaluations',
        'Total evaluations',
        'R2 (train)',
        'R2 (test)']

header = meta_header + output_header

df = pd.DataFrame(columns=header)
parameter_space = list(itertools.product(population_size, iteration_count, evaluation_budget))
data_files = list(os.listdir(data_path)) if os.path.isdir(data_path) else [ os.path.basename(data_path) ]
data_count = len(data_files)

coloredlogs.install(stream=sys.stdout, level=logging.INFO)
logger = logging.getLogger("operon-osgp")

for pop_size, iter_count, eval_count in parameter_space:
    logger.info('Population size: {}, iterations: {}, evaluation budget: {}'.format(pop_size, iter_count, eval_count))
    
    for i,f in enumerate(data_files):
        with open(os.path.join(base_path, f), 'r') as h:
            lines = h.readlines()
            count = len(lines) - 1
            train = int(count * 2 / 4)
            target = lines[0].split(',')[-1].strip('\n')
            problem_name = os.path.splitext(f)[0]
            logger.info('Problem [{}/{}]\t{}\tRows: {}\tTarget: {}'.format(i+1, data_count, problem_name, train, target))

            df2 = pd.DataFrame(columns=header)

            for j in range(reps):
                output = subprocess.check_output([bin_path, 
                    "--dataset", os.path.join(base_path, f), 
                    "--target", target, 
                    "--train", '0:{}'.format(train), 
                    "--selection-pressure", str(100), 
                    "--iterations", str(iter_count), 
                    "--evaluations", str(eval_count), 
                    "--population-size", str(pop_size),
                    "--enable-symbols", "exp,log,sin,cos"]);

                n = df.shape[0]


                lines = list(filter(lambda x: x, output.split(b'\n')))
                result = '\t'.join([v.decode('ascii') for v in lines[-1].split(b'\t') ])
                logger.info('[{:#2d}/{}]\t{}\t{}'.format(j+1, reps, problem_name, result))

                meta = [ problem_name, pop_size, iter_count, eval_count, j+1 ]
                df2.loc[j] = meta  + [ np.nan if v == 'nan' else float(v) for v in lines[-1].split(b'\t') ]

                for i,line in enumerate(lines):
                    vals = [ np.nan if v == 'nan' else float(v) for v in line.split(b'\t') ]
                    df.loc[i + n] = meta + vals

            for l in str(df2.median(axis=0).T).split('\n'):
                logger.info(l)
            df2.to_excel('OSGP_{}_{}_{}_{}.xlsx'.format(problem_name, pop_size, iter_count, eval_count))
                        
df.to_excel('OSGP.xlsx')
