def calculator(a,b,operation):
    res = None
    if operation == "+":
        res = a + b
    elif operation == "-":
        res = a - b
    elif operation == "*":
        res = a * b
    elif operation == "/":
        if b == 0:
            res = "Division by Zero"
        else:
            res = a / b
    else:
        res = "invalid operation" 

    return res

while 1 == 1:

    a = int(input("a:"))
    b = int(input("b:"))
    op = str(input("operation:"))

    res = calculator(a,b,op)
    print(res)
    
    print("type 'q' for exit")
    q = input()
    if q == "q":
        break