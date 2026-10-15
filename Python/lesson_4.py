with open("test.txt","a") as w_file:
    while True:
        new_line = input("<")

        if new_line == "q":
            break
        else:
            w_file.write(f"\n{new_line}")


with open("test.txt","r") as r_file:
    print(r_file.read())

