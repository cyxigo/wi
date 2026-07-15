import time


start = time.process_time()
list = []
for i in range(0, 1000000):
    list.append(i)

sum = 0
for i in list:
    sum += i
print(sum)
print("elapsed: " + str(time.process_time() - start))
