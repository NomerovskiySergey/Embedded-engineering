def calculate_average(scores):
    if len(scores) == 0:
        return 0
    return sum(scores) / len(scores);

name = input("Student name: ")
scores_commastr = input("Student scores: ")

scores_int = [int(x) for x in scores_commastr.split(',') if x.strip().isdigit()]
average_score = calculate_average(scores_int)

final_result = f"{name} has average score: {average_score} and {"pass" if average_score > 4 else "fail"} the course "
print(final_result)