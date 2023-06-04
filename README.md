# GameOfLife_MPI
Начальную конфигурацию клеток программа берёт из data.txt
В первой строчке высота и ширине поля, через пробел, в последующий строчках координаты живых клеток - x y.
Так как система, на которой проводиль замеры, поддерживает всего 4 потока, то разбивание на большее число потоков не улучшает результат.
Уменьшение времени максимум в 3,7 раза на 4 потоках.
# График
![alt text](https://github.com/Konev-Denis/GameOfLife_MPI/blob/main/chart.png?raw=true)
![alt text](https://github.com/Konev-Denis/GameOfLife_MPI/blob/main/tabl.png?raw=true)
