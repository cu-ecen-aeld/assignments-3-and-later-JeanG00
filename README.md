# aesd-assignments
This repo contains public starter source code, scripts, and documentation for Advanced Embedded Software Development (ECEN-5713) and Advanced Embedded Linux Development assignments University of Colorado, Boulder.

## Setting Up env


```bash
# install dependencies
sudo apt-get install -y build-essential ruby cmake git openssh-server vim

# setup cross-compiler
sudo tar xJf arm-gnu-toolchain-13.3.rel1-x86_64-aarch64-none-linux-gnu.tar.xz -C /usr/local/arm/

# export env
echo "if [ -d /usr/local/arm/arm-gnu-toolchain-13.3.rel1-x86_64-aarch64-none-linux-gnu/bin ]; then \n PATH=\"$PATH:/usr/local/arm/arm-gnu-toolchain-13.3.rel1-x86_64-aarch64-none-linux-gnu/bin\" \n fi" >> ~/.bashrc
source ~/.bashrc

mkdir -p assignments/assignment
aarch64-none-linux-gnu-gcc -v -print-sysroot > assignments/assignment2/cross-compile.txt
```

## [Testing](https://github.com/cu-ecen-aeld/assignment-autotest/)

```bash
# either follow previous
git remote rm origin
git remote add origin git@github.com:cu-ecen-aeld/assignments-3-and-later-JeanG00.git
# or create new repo
git clone git@github.com:cu-ecen-aeld/assignments-3-and-later-JeanG00.git

git remote add assignments-base git@github.com:cu-ecen-aeld/aesd-assignments.git
git fetch assignments-base
git merge assignments-base/<branchname>
git submodule update --init --recursive
# ... DO HOMEWORKS
git commit -am "feat: assignments done"
git push -u origin main
```

## Assignments Grading Criteria

### [assignment-1-instructions](https://www.coursera.org/learn/linux-system-programming-introduction-to-buildroot/supplement/bnixD/assignment-1-instructions)

### [assignment-2-instructions](https://www.coursera.org/learn/linux-system-programming-introduction-to-buildroot/supplement/U1Beh/assignment-2-instructions)

### [assignment-3-part-1-instructions](https://www.coursera.org/learn/linux-system-programming-introduction-to-buildroot/supplement/Nh4LM/assignment-3-part-1-instructions)

### [assignment-3-part-2-instructions](https://www.coursera.org/learn/linux-system-programming-introduction-to-buildroot/supplement/YGf42/assignment-3-part-2-instructions)

### [assignment-4-part-1-instructions](https://www.coursera.org/learn/linux-system-programming-introduction-to-buildroot/supplement/GT0Ld/assignment-4-part-1-instructions)
