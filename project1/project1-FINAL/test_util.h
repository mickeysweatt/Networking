#include <cstdio>
#include <string>
#include <iostream>
#include <cerrno>
#include <fstream>

static int testStatus = 0;

static void aSsErT(int c, const char *s, int i) {
    if (c) {
        std::cout << "Error " << __FILE__ << "(" << i << "): " << s
             << "    (failed)" << std::endl;
        if (testStatus >= 0 && testStatus <= 100) ++testStatus;
    }
}

#define ASSERT(X) { aSsErT(!(X), #X, __LINE__); }

//=============================================================================
//                    STANDARD LOOP-ASSERT TEST MACROS
//-----------------------------------------------------------------------------

#define LOOP_ASSERT(I,X) { \
    if (!(X)) { std::cout << #I << ": " << I << "\n"; aSsErT(1, #X, __LINE__);}}

#define LOOP2_ASSERT(I,J,X) { \
    if (!(X)) { std::cout << #I << ": " << I << "\t" << #J << ": " \
              << J << "\n"; aSsErT(1, #X, __LINE__); } }

#define LOOP3_ASSERT(I,J,K,X) { \
   if (!(X)) { std::cout << #I << ": " << I << "\t" << #J << ": " << J << "\t" \
              << #K << ": " << K << "\n"; aSsErT(1, #X, __LINE__); } }

#define LOOP4_ASSERT(I,J,K,L,X) { \
   if (!(X)) { std::cout << #I << ": " << I << "\t" << #J << ": " << J << "\t" << \
       #K << ": " << K << "\t" << #L << ": " << L << "\n"; \
       aSsErT(1, #X, __LINE__); } }

//=============================================================================
//                      SEMI-STANDARD TEST OUTPUT MACROS
//-----------------------------------------------------------------------------

#define P(X) std::cout << #X " = " << (X) << std::endl; // Print identifier and value.
#define Q(X) std::cout << "<| " #X " |>" << std::endl;  // Quote identifier literally.
#define P_(X) std::cout << #X " = " << (X) << ", " << std::flush; // P(X) without '\n'
#define A(X) std::cout << #X " = " << ((void *) X) << std::endl;  // Print address
#define A_(X) std::cout << #X " = " << ((void *) X) << ", " << flush;
#define L_ __LINE__                           // current Line number
#define TAB std::cout << '\t';
#define T_ TAB                               // prints a single tab to the screen


static std::string get_file_contents(const char *filename)
{
  std::ifstream in(filename, std::ios::in | std::ios::binary);
  if (in)
  {
    std::string contents;
    in.seekg(0, std::ios::end);
    contents.resize(in.tellg());
    in.seekg(0, std::ios::beg);
    in.read(&contents[0], contents.size());
    in.close();
    return(contents);
  }
  throw(errno);
}