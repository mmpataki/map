bold=$(tput bold)
normal=$(tput sgr0)

echo_and_exec() {
    echo
    echo "${bold}Running $* ${normal}"
    $*
}

echo_and_exec gcc lib.c map.c -o map
echo_and_exec gcc lib.c unmap.c -o unmap

echo_and_exec ./map test.mzip ./x ./y

echo_and_exec ./unmap -l test.mzip
echo_and_exec ./unmap -d test.mzip
echo_and_exec ./unmap test.mzip
