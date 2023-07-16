// MIT License
// 
// Copyright (c) 2023 Trevor Bakker 
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <time.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <getopt.h>
#include <stdint.h>
#include "utility.h"
#include "star.h"
#include "float.h"
#include <pthread.h>

#define NUM_STARS 30000
#define MAX_LINE 1024
#define DELIMITER " \t\n"

struct Star star_array[NUM_STARS];
uint8_t (*distance_calculated)[NUM_STARS];

double min = FLT_MAX;
double max = FLT_MIN;
double dist = 0.0;
uint64_t count = 0;

uint32_t thread_count = 1;
pthread_mutex_t mutex;

int num_threads, c;

void showHelp()
{
  printf("Use: findAngular [options]\n");
  printf("Where options are:\n");
  printf("-t          Number of threads to use\n");
  printf("-h          Show this help\n");
}

float determineAverageAngularDistance(struct Star arr[], int start, int end)
{
  uint32_t i, j;

  for (i = start; i < end; i++)
  {
    for (j = 0; j < NUM_STARS; j++)
    {
      if (i != j && distance_calculated[i][j] == 0)
      {
        double dist = calculateAngularDistance(arr[i].RightAscension, arr[i].Declination,
                                               arr[j].RightAscension, arr[j].Declination);
        distance_calculated[i][j] = 1;
        distance_calculated[j][i] = 1;
        pthread_mutex_lock(&mutex);

        count++;

        if (min > dist)
        {
          min = dist;
        }

        if (max < dist)
        {
          max = dist;
        }
        pthread_mutex_unlock(&mutex);
      }
    }
  }
  return dist;
}

void *main_thread(void *a)
{
  int p = (intptr_t)a;
  c = p;
  int start = 0, end = 0;
  int interval = NUM_STARS / num_threads;

  start = interval * c;
  end = interval * (c + 1);
  determineAverageAngularDistance(star_array, start, end);
  return NULL;
}

int main(int argc, char *argv[])
{
  FILE *fp;
  uint32_t star_count = 0;

  uint32_t n;

  distance_calculated = malloc(sizeof(uint8_t[NUM_STARS][NUM_STARS]));

  if (distance_calculated == NULL)
  {
    uint64_t num_stars = NUM_STARS;
    uint64_t size = num_stars * num_stars * sizeof(uint8_t);
    printf("Could not allocate %ld bytes\n", size);
    exit(EXIT_FAILURE);
  }

  memset(distance_calculated, 0, sizeof(uint8_t[NUM_STARS][NUM_STARS]));

  for (n = 1; n < argc; n++)
  {
    if (strcmp(argv[n], "-help") == 0)
    {
      showHelp();
      exit(0);
    }
    else if (strcmp(argv[n], "-t") == 0)
    {
      num_threads = atoi(argv[n + 1]);
    }
  }

  fp = fopen("data/tycho-trimmed.csv", "r");

  if (fp == NULL)
  {
    printf("ERROR: Unable to open the file data/tycho-trimmed.csv\n");
    exit(1);
  }

  char line[MAX_LINE];
  while (fgets(line, 1024, fp))
  {
    uint32_t column = 0;

    char *tok;
    for (tok = strtok(line, " "); tok && *tok; tok = strtok(NULL, " "))
    {
      switch (column)
      {
      case 0:
        star_array[star_count].ID = atoi(tok);
        break;

      case 1:
        star_array[star_count].RightAscension = atof(tok);
        break;

      case 2:
        star_array[star_count].Declination = atof(tok);
        break;

      default:
        printf("ERROR: line %d had more than 3 columns\n", star_count);
        exit(1);
        break;
      }
      column++;
    }
    star_count++;
  }
  printf("%d records read\n", star_count);
  pthread_t threads[num_threads];
  clock_t clockNumber;
  clockNumber = clock();
  // Find the average angular distance in the most inefficient way possible
  clock_t start = clock();
  pthread_mutex_init(&mutex, NULL);
  for (int i = 0; i < num_threads; i++)
  {
    pthread_create(&threads[i], NULL, main_thread, (void *)(intptr_t)i);
  }
  for (int i = 0; i < num_threads; i++)
  {
    pthread_join(threads[i], NULL);
  }
  clock_t end = clock();

  double distance_sum = 0.0;
  uint64_t count = 0;
  for (int i = 0; i < NUM_STARS; i++)
  {
    for (int j = i + 1; j < NUM_STARS; j++)
    {
      if (distance_calculated[i][j])
      {
        double dist = calculateAngularDistance(
            star_array[i].RightAscension, star_array[i].Declination,
            star_array[j].RightAscension, star_array[j].Declination);
        distance_sum += dist;
        count++;
      }
    }
  }

  double average_distance = distance_sum / count;

  printf("Average distance found is %lf\n", average_distance);
  printf("Minimum distance found is %lf\n", min);
  printf("Maximum distance found is %lf\n", max);
  double time = ((double)(end - start)) / CLOCKS_PER_SEC;
  printf("Time spent: %f seconds \n", time);

  fclose(fp);
  pthread_exit(NULL);

  return 0;
}