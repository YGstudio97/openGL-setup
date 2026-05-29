import os

file = input("Enter cpp file: ")
output = input("Enter output name: ")

cmd = (
    f"g++ -g --std=c++17 "
    f"-I../include "
    f"-L../lib "
    f"{file} "
    f"../src/glad.c "
    f"-lglfw3dll "
    f"-o {output}"
)

print("\nRunning:\n")
print(cmd)

os.system(cmd)