#include <fstream>
#include <iostream>
#include <string>
#include "mpi.h"
#include <windows.h>

using namespace std;

const int COUNT_STEPS_EVOLUTION = 100;

const char DEAD_CELL = '-';
const char LIVE_CELL = '*';

char** createWorld(int height, int width) {
	char** world = new char* [height];
	for (int i = 0; i < height; i++) {
		world[i] = new char[width];
		std::fill_n(world[i], width, DEAD_CELL);
	}
	return world;
}

void printWorld(int height, int width, char** world) {
	for (int i = 0; i < height; i++) {
		for (int j = 0; j < width; j++)
			std::cout << world[i][j];
		std::cout << std::endl;
	}
}

int getLiveNeighbours(int height, int width, char** world, int x, int y) {
	int count = 0;
	for (int i = x - 1; i <= x + 1; i++)
		for (int j = y - 1; j <= y + 1; j++) {
			if ((i == x && j == y) || i < 0 || i >= height || j < 0 || j >= width)
				continue;
			if (world[i][j] == LIVE_CELL)
				count++;
		}
	return count;
}

char** next_generation_world(int height, int width, char** world) {
	char** new_world = createWorld(height, width);
	for (int i = 0; i < height; i++) {
		for (int j = 0; j < width; j++) {
			const int live_nb = getLiveNeighbours(height, width, world, i, j);
			if ((live_nb == 3) || (live_nb == 2 && world[i][j] == LIVE_CELL))
				new_world[i][j] = LIVE_CELL;
			else
				new_world[i][j] = DEAD_CELL;
		}
	}
	return new_world;
}

int main(int argc, char** argv)
{
	int size, rank;
	int height, width;
	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &size);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	double Time_work = MPI_Wtime();
	if (rank == 0)
	{
		std::ifstream input_data("data.txt");
		input_data >> height >> width;
		char** world = createWorld(height, width);

		int x, y;
		while (input_data >> x >> y)
			world[x][y] = LIVE_CELL;

		input_data.close();

		MPI_Status status;
		char* send_bufer = new char[width];
		char* recv_bufer = new char[width];

		int shift = height % size;
		for (int k = 1; k < size; k++) {
			// отправляем размеры
			int* loc_size = new int[2]{height / size + 2, width};
			MPI_Send(loc_size, 2, MPI_INT, k, 1, MPI_COMM_WORLD);
			// отправляем участок мира по одной строчке, так безопаснее
			for (int i = 1; i < height / size + 1; i++) {
				for (int j = 0; j < width; j++)
					recv_bufer[j] = world[shift + (height / size * k) + i - 1][j];
				MPI_Send(recv_bufer, width, MPI_CHAR, k, 2, MPI_COMM_WORLD);
			}
			//std::cout << "Send " << rank << " to " << k << " count " << height / size + 2 << std::endl;
		}
		// копируем свой участок мира
		int loc_height = height / size + height % size + 1;
		char** loc_word = createWorld(loc_height, width);
		for (int i = 0; i < loc_height; i++)
			for (int j = 0; j < width; j++)
				loc_word[i][j] = world[i][j];
		double Time_work_iter = MPI_Wtime();
		for (int gen = 1; gen <= COUNT_STEPS_EVOLUTION; gen++) {
			// обмен данными с соседним участком
			// отправляем правому
			for (int j = 0; j < width; j++)
				send_bufer[j] = loc_word[loc_height - 2][j];
			MPI_Send(send_bufer, width, MPI_CHAR, rank + 1, 1, MPI_COMM_WORLD);
			// принимаем от правого
			MPI_Recv(recv_bufer, width, MPI_CHAR, rank + 1, 1, MPI_COMM_WORLD, &status);
			for (int j = 0; j < width; j++)
				loc_word[loc_height - 1][j] = recv_bufer[j];

			char** new_world = next_generation_world(loc_height, width, loc_word);
			loc_word = new_world;
			if (gen == 1) {
				Time_work_iter = MPI_Wtime() - Time_work_iter;
				cout << "Total iter time = " << Time_work_iter << endl;
			}
		}
		// сбор даынных со всех потоков
		for (int i = 0; i < loc_height; i++)
			for (int j = 0; j < width; j++)
				world[i][j] = loc_word[i][j];
		for (int k = 1; k < size; k++) {
			for (int i = 1; i < height / size + 1; i++) {
				MPI_Recv(recv_bufer, width, MPI_CHAR, k, 1, MPI_COMM_WORLD, &status);
				for (int j = 0; j < width; j++)
					world[shift + (height / size * k) + i - 1][j] = recv_bufer[j];
			}
			//std::cout << "Send " << rank << " to " << k << " count " << height / size + 2 << std::endl;
		}
		//printWorld(height, width, world);
		std::ofstream out("res.txt", std::ofstream::out | std::ofstream::trunc);
		if (out.is_open())
		{
			out << height << " " << width << std::endl;
			for (int i = 0; i < height; i++)
				for (int j = 0; j < width; j++)
					if (world[i][j] == LIVE_CELL)
						out << i << " " << j << std::endl;
		}
		out.close();
	}
	else
	{
		int count;
		MPI_Status status;
		// принимаем размеры
		MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
		MPI_Get_count(&status, MPI_INT, &count);
		int* size_world = new int[count];
		MPI_Recv(size_world, count, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
		height = size_world[0];
		width = size_world[1];
		char* send_bufer = new char[width];
		char* recv_bufer = new char[width];
		// принимаем участок мира
		char** world = createWorld(height, width);
		for (int i = 1; i < height - 1; i++) {
			MPI_Recv(recv_bufer, width, MPI_CHAR, 0, 2, MPI_COMM_WORLD, &status);
			for (int j = 0; j < width; j++)
				world[i][j] = recv_bufer[j];
		}

		for (int gen = 1; gen <= COUNT_STEPS_EVOLUTION; gen++) {
			// обмен данными между соседними участками
			if (rank != size - 1) {
				// отправляем правому, принимаем от левого
				for (int j = 0; j < width; j++)
					send_bufer[j] = world[height - 2][j];
				MPI_Sendrecv(send_bufer, width, MPI_CHAR, rank + 1, 1, recv_bufer, width, MPI_CHAR, rank - 1, 1, MPI_COMM_WORLD, &status);
				for (int j = 0; j < width; j++)
					world[0][j] = recv_bufer[j];
				// отправляем левому, принимаем от правого
				for (int j = 0; j < width; j++)
					send_bufer[j] = world[1][j];
				MPI_Sendrecv(send_bufer, width, MPI_CHAR, rank - 1, 1, recv_bufer, width, MPI_CHAR, rank + 1, 1, MPI_COMM_WORLD, &status);
				for (int j = 0; j < width; j++)
					world[height - 1][j] = recv_bufer[j];
			}
			else
			{
				// принимаем от левого
				MPI_Recv(recv_bufer, width, MPI_CHAR, rank - 1, 1, MPI_COMM_WORLD, &status);
				for (int j = 0; j < width; j++)
					world[0][j] = recv_bufer[j];
				// отправляем левому
				for (int j = 0; j < width; j++)
					send_bufer[j] = world[1][j];
				MPI_Send(send_bufer, width, MPI_CHAR, rank - 1, 1, MPI_COMM_WORLD);
			}
			
			char** new_world = next_generation_world(height, width, world);
			world = new_world;
		}
		// отправляем участок мира 
		for (int i = 1; i < height - 1; i++) {
			for (int j = 0; j < width; j++)
				send_bufer[j] = world[i][j];
			MPI_Send(send_bufer, width, MPI_CHAR, 0, 1, MPI_COMM_WORLD);
		}
	}
	Time_work = MPI_Wtime() - Time_work;
	if (rank == 0) {
		cout << "Total time = " << Time_work << endl;
	}
	MPI_Finalize();
	return 0;
}