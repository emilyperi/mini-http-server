# Mini HTTP Server
This project is from UC Berkeley's Operating Systems course. The assignment was to create a simple HTTP server in C that can serve GET requests with and without a worker thread pool. I originally did this project in Summer 2019, but lost my code in a broken virtual box. Since then, the project has changed a bit (now they are doing it in Rust) and this one's skeleton code comes from the spring 2022 iteration.

The parts that were assigned are designated by the `PART X BEGIN` and `PART X END.` If you are interested in trying it out yourself, you can visit the [course homepage](https://inst.eecs.berkeley.edu/~cs162/sp22/). The vagrant box and skeleton code are available on [github](https://github.com/Berkeley-CS162/student0-sp22) (as of writing this).

## Usage
Compiling with `make` will create two executables, `httpserver` and `poolserver`

To run the basic server:
```
./httpserver --files any_directory_with_files/ [--port 8000]
```
If no port is specified, it will default to 8000. If you are running in the CS162 vagrant box, you will have to configure port forwarding in your Vagrantfile in order to access the served files on your host browser. To do this, in your Vagrantfile, uncomment/add:
```
config.vm.network "forwarded_port", guest: 8000, host: 8000, host_ip: "127.0.0.1"
```

To run the pool server:
```
./poolserver --files any_directory_with_files/ --port 8000 --num-thread [some number]
```

Finally, another component of the assignment is to run a proxy-server:
```
./httpserver --proxy inst.eecs.berkeley.edu:80 [--port 8000]
```
