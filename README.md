# vktest

```
git clone --recurse-submodules https://github.com/Jonathan-Weinstein/vktest.git
git pull --recurse-submodules
cd vktest
make
```

Alternative to make without caching anything:
`g++ -DVK_NO_PROTOTYPES  *.cpp -std=c++11 -Wall -Wshadow -ldl -o vktest.out`

Run in the repo directory via:

`./vktest.out [--gpuindex=%d] [--test=%s] [--save-failing-images]`
