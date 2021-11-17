# vktest

When cloning, something like --recurse-submodules may be needed to pull in volk.

Can be built like this in the repo directory:

`g++ -DVK_NO_PROTOTYPES  *.cpp -std=c++11 -Wall -Wshadow -ldl -o vktest.out`

Run in the repo directory via:

`./vktest [gpu_index (default = 0)]`

The program must be able to find and read a file "reference_2x.png" from where
it is run.

"Test PASSED." is printed on success. Otherwise, the written "generated_2x.png"
file can manually be inspected and diffed against the reference image.
