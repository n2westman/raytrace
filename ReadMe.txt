========================================================================
    CS174A Project 3 - A simple ray tracer
========================================================================

Contents of the project:

- template-rt.cpp: main source file
- matm.h: modified mat.h from Edward Angel. Code to invert a matrix 
  was added there (function InvertMatrix).
- vecm.h: modified vec.h from Edward Angel. A vec4 != operator was added here to give a better sense of floating point equality

=========================================================================

March 13, 2014

Ray Tracer w/ basic Phong shading and 5-level reflection recursion. Handles spheres because they have simple mathematical properties and easily calculated normals.

Does not transmit light (outside the scope of this project). It's a fun little project.

To generate, it parses a text file and generates an appropriate image.

Please include -fopenmp flag if compiling on gcc, since the code uses omp to get some decent calculation speedup. It can't render in real time, but it was nice to speed up development time a bit.

Usage: template-rt <input_file.txt>

Executable File is found in Debug/