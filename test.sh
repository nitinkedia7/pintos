cd pintos/src/examples/
make clean
make
cd ../userprog
make clean
make
cd build
pintos-mkdisk fs.dsk 2
pintos -q -f
pintos -p ../../examples/echo -a echo -- -q
pintos -q run "echo My stack_setup() works"
