#OpenSourceStuff

This is a repository where I gather small usefull functions of mine that I've decided to realase for free, currently only involving DH_memset_32.

### DH_memset_32
DH_memset_32 is a fast implementation of memset for 4 byte values.
Letting memset accept integer values is totally reasonable, its less limiting and doesn't cost anything.

On my machene my implementation is actually faster than the default memset on both clang and msvc, more than twice as fast in certain ranges and never slower. 

Why not accept 8 byte values as well?
It probably should but I haven't had the need for one so I havn't written it. I might though.

##Liscense for everything in this repository is:
  This software is dual-licensed to the public domain and under the following
  license: you are granted a perpetual, irrevocable license to copy, modify,
  publish, and distribute this file as you see fit.
  
  
  
##Who am I?
  
  I'm a university student at Chalmers University of Technology. When I get too bored with java design patterns and other 'useful' stuff we have to do in school I usually do some c programming.
  
##Why should I trust some collage student that he can write correct code?
 You probably shouldn't at least not if something is depending on it. Verify that the implimentation is correct youself. 
  
