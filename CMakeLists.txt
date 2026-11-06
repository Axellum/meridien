cmake_minimum_required(VERSION 3.16)

# Les composants du projet sont dans components/ (noyau pur, pilotes, interface, couche carte).
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ecran_autonome)
