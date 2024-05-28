/* This file is used by configure to set the paths for the
   the button images and colored balls (IMAGESOURCE),
   the base definitions (BASEDEF), and the tools to convert
   postscript files to gif files
*/

/* There are two kinds of source paths for the pre-defined images
   One uses a location NOT available to the Web and copies the files into
   the same directory that is holding the generated HTML files.  The
   code generates RELATIVE locations for the images.

   The other uses a location that IS available to the Web, and generates
   ABSOLUTE locations
 */

#ifndef PSPATH
#define PSPATH "/home/jiping/FENGSim/toolkit/NLA/petsc/arch-linux-c-debug/bin/"
#endif

#ifndef BASEDEF
#define BASEDEF "/home/jiping/FENGSim/toolkit/NLA/petsc/arch-linux-c-debug/share/basedefs.txt"
#endif

#ifndef IMAGEDIR
#define IMAGEDIR "/home/jiping/FENGSim/toolkit/NLA/petsc/arch-linux-c-debug/share/"
#endif

/* This can be set to the base URL for a directory that contains the image
   files */
#ifndef IMAGEURL
#define IMAGEURL ""
#endif
