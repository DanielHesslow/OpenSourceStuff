#OpenSourceStuff

This is a repository where I gather small usefull functions of mine that I've decided to realase for free, currently involving DH_memset_32, and example code for using raw_input on windows.



### DH_RingBuffer
DH_RingBuffer is a fast fixed-size threadsafe and lock-free queue
It can easily be modified to allow for dynamic sizing at the expence of the lock-free-ness.
it would _probably_ not be too much of an overhead because it's an edgecase. 


### DH_memset_32
DH_memset_32 is a fast implementation of memset for 4 byte values.
Letting memset accept integer values is totally reasonable, its less limiting and doesn't cost anything.

On my machene my implementation is actually *faster* than the default memset on both clang and msvc (havn't tested on gcc), more than twice as fast in certain ranges and never slower. This is beacuse we don't need to set the final values since we're allways setting whole 4 bytes. Which saves a couple of instructions and reduces branches slightly. Memset is also memory bound so it's not hard to make fast.

**Why not accept 8 byte values as well?**
It probably should but I haven't had the need for one so I havn't written it. I might though.

**Why did you need a fast 4 byte memset?**
I'm currently working on a software rendendered texteditor. If we want to set a block to some color (eg. background) we need to have a fast memset, if that color isn't grayscale it needs to accept 4 byte values.


### raw_input_example.cpp
Is just some small example code of how to use raw_input on windows to get left/right -control/-alt/-shift/-enter, listning to keypresses in the background and getting out the unicode characters (without going through WM_UNICODE). I actually think using raw_input results in  code that is slightly cleaner that handeling all WM_CHAR,WM_UNICODE,WM_KEY,WM_SYSKEY ... (and maybe even GetKeyBoardState GetKey GetKeyAsync) etc...  



##Liscense for everything in this repository is:
  This software is dual-licensed to the public domain and under the following
  license: you are granted a perpetual, irrevocable license to copy, modify,
  publish, and distribute this file as you see fit.
  
  
  
##Who are you?
  
  I'm a university student at Chalmers University of Technology in Sweden. When I get too bored with java design patterns and other 'useful' stuff we have to do in school I usually do some c programming.
  
##Why should I trust some collage student that he can write correct code?
 You probably shouldn't at least not if something is depending on it. Verify that the implimentation is correct youself. 
  
