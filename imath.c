#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <sys/time.h>
#include <pthread.h>
#include <time.h>

#define THREADS 5

#define filterWidth 3
#define filterHeight 3

#define RGB_MAX 255

typedef struct
{
    unsigned char r, g, b;
} PPMPixel;

typedef struct
{
    PPMPixel *image;         //original image
    PPMPixel *result;        //filtered image
    unsigned long int w;     //width of image
    unsigned long int h;     //height of image
    unsigned long int start; //starting point of work
    unsigned long int size;  //equal share of work (almost equal if odd)
} parameter;

pthread_mutex_t lock;

/*This is the thread function. It will compute the new values for the region of image specified in params (start to start+size) using convolution.
    (1) For each pixel in the input image, the filter is conceptually placed on top of the image with its origin lying on that pixel.
    (2) The  values  of  each  input  image  pixel  under  the  mask  are  multiplied  by the corresponding filter values.
    (3) The results are summed together to yield a single output value that is placed in the output image at the location of the pixel being processed on the input.
*/
void *threadfn(void *params)
{
    pthread_mutex_lock(&lock);

    parameter temp = *((parameter *)params);

    int imageHeight = temp.h;
    int imageWidth = temp.w;

    int iteratorFilterHeight;
    int iteratorFilterWidth;
    int filterIterator;

    int iteratorImageWidth = 0;
    int iteratorImageHeight = temp.start;

    int counter = 0;
    int start = temp.start;
    int size = temp.size;

    int x_coordinate = 0;
    int y_coordinate = 0;

    int red;
    int green;
    int blue;

    int laplacian[filterWidth][filterHeight] =
        {-1, -1, -1, -1, 8, -1, -1, -1, -1};

    while (iteratorImageHeight < (start + size))
    {
        iteratorFilterHeight = 0;
        iteratorFilterWidth = 0;
        filterIterator = 0;
        red = 0;
        green = 0;
        blue = 0;

        while (filterIterator != 9)
        {
            x_coordinate = (iteratorImageWidth - filterWidth / 2 + iteratorFilterWidth + imageWidth) % imageWidth;
            y_coordinate = (iteratorImageHeight - filterHeight / 2 + iteratorFilterHeight + imageHeight) % imageHeight;

            red += (temp.image[y_coordinate * imageWidth + x_coordinate].r * laplacian[iteratorFilterHeight][iteratorFilterWidth]);
            green += (temp.image[y_coordinate * imageWidth + x_coordinate].g * laplacian[iteratorFilterHeight][iteratorFilterWidth]);
            blue += (temp.image[y_coordinate * imageWidth + x_coordinate].b * laplacian[iteratorFilterHeight][iteratorFilterWidth]);

            if (iteratorFilterWidth == 2)
            {
                iteratorFilterWidth = 0;
                ++iteratorFilterHeight;
            }
            else
                ++iteratorFilterWidth;
            ++filterIterator;
        }

        if (red < 0)
            red = 0;
        if (red > 255)
            red = 255;

        if (green < 0)
            green = 0;
        if (green > 255)
            green = 255;

        if (blue < 0)
            blue = 0;
        if (blue > 255)
            blue = 255;

        temp.result[iteratorImageHeight * imageWidth + iteratorImageWidth].r = red;
        temp.result[iteratorImageHeight * imageWidth + iteratorImageWidth].g = green;
        temp.result[iteratorImageHeight * imageWidth + iteratorImageWidth].b = blue;

        if (iteratorImageWidth == (imageWidth - 1))
        {
            iteratorImageWidth = 0;
            ++iteratorImageHeight;
        }
        else
            ++iteratorImageWidth;

        ++counter;
    }
    pthread_mutex_unlock(&lock);

    return NULL;
}

/*Create a new P6 file to save the filtered image in. Write the header block
 e.g. P6
      Width Height
      Max color value
 then write the image data.
 The name of the new file shall be "name" (the second argument).
 */
void writeImage(PPMPixel *image, char *name, unsigned long int width, unsigned long int height)
{
    FILE *outFile = fopen(name, "w");

    fprintf(outFile, "P6 ");
    fprintf(outFile, "%ld ", width);
    fprintf(outFile, "%ld ", height);
    fprintf(outFile, "%d\n", RGB_MAX);

    if (fwrite(image, sizeof(PPMPixel), width * height, outFile) != (width * height))
    {
        fprintf(stderr, "fwrite in writeImage failed\n");
        exit(1);
    }

    fclose(outFile);
}

/* Open the filename image for reading, and parse it.
    Example of a ppm header:    //http://netpbm.sourceforge.net/doc/ppm.html
    P6                  -- image format
    # comment           -- comment lines begin with
    ## another comment  -- any number of comment lines
    200 300             -- image width & height
    255                 -- max color value
 
 Check if the image format is P6. If not, print invalid format error message.
 Read the image size information and store them in width and height.
 Check the rgb component, if not 255, display error message.
 Return: pointer to PPMPixel that has the pixel data of the input image (filename)
 */
PPMPixel *readImage(const char *filename, unsigned long int *width, unsigned long int *height)
{
    int headCounter = 0;
    char width_arr[50];
    char height_arr[50];
    int temp;

    PPMPixel *img;
    FILE *infile = fopen(filename, "r+");

    temp = fgetc(infile);

    while (1)
    {
        //Check for ' ' or '\n'
        if (isspace(temp))
        {
            while (isspace(temp))
            {
                temp = fgetc(infile);
            }

            if (temp == '\0')
            {
                fprintf(stderr, "Not enough information in header of PPM file\n");
                exit(1);
            }
        }
        //Check for comments
        else if (temp == '#')
        {
            while (temp != '\n' && temp != '\0')
            {
                temp = fgetc(infile);
            }

            if (temp == '\0')
            {
                fprintf(stderr, "Not enough information in header of PPM file\n");
                exit(1);
            }
        }
        //Check or obtain header values
        else
        {
            switch (headCounter)
            {
            case 0:
                if (temp == 'P')
                {
                    temp = fgetc(infile);

                    if (temp == '6')
                    {
                        ++headCounter;
                    }
                    else
                    {
                        fprintf(stderr, "Wrong input file\n");
                        exit(1);
                    }
                }
                else
                {
                    fprintf(stderr, "Wrong input file\n");
                    exit(1);
                }
                break;

            case 1:
                for (int i = 0; !isspace(temp); ++i)
                {
                    width_arr[i] = temp;
                    temp = fgetc(infile);
                }

                *width = atoi(width_arr);
                ++headCounter;
                break;

            case 2:
                for (int i = 0; !isspace(temp); ++i)
                {
                    height_arr[i] = temp;
                    temp = fgetc(infile);
                }

                *height = atoi(height_arr);
                ++headCounter;
                break;

            case 3:
                if (temp == '2')
                {
                    temp = fgetc(infile);
                    if (temp == '5')
                    {
                        temp = fgetc(infile);
                        if (temp == '5')
                        {
                            ++headCounter;
                        }
                        else
                        {
                            fprintf(stderr, "RGB max is not 255\n");
                            exit(1);
                        }
                    }
                    else
                    {
                        fprintf(stderr, "RGB max is not 255\n");
                        exit(1);
                    }
                }
                else
                {
                    fprintf(stderr, "RGB max is not 255\n");
                    exit(1);
                }
                break;

            default:
                break;
            }

            temp = fgetc(infile);
        }

        if (headCounter == 4)
        {
            break;
        }
    }

    img = malloc((*width) * (*height) * 3);

    if (fread(img, 3, (*height) * (*width), infile) != ((*height) * (*width)))
    {
        fprintf(stderr, "fread in readImage wasn't successful\n");
        exit(1);
    }

    fclose(infile);

    return img;
}

/* Create threads and apply filter to image.
 Each thread shall do an equal share of the work, i.e. work=height/number of threads.
 Compute the elapsed time and store it in *elapsedTime (Read about gettimeofday).
 Return: result (filtered image)
 */
PPMPixel *apply_filters(PPMPixel *image, unsigned long w, unsigned long h, double *elapsedTime)
{
    clock_t start, end;
    start = clock();

    pthread_t tid[THREADS];
    int work = 0;
    int remainWork = 0;
    int checker = 0;

    PPMPixel *result = malloc(sizeof(PPMPixel) * (w * h));
    parameter *params = malloc(sizeof(parameter) * THREADS);

    if (h % THREADS == 0)
    {
        work = h / THREADS;
    }
    else if (THREADS != 1)
    {
        checker = 1;
        remainWork = h % (THREADS - 1);
        work = (h - remainWork) / (THREADS - 1);
    }
    else
    {
        work = h;
    }

    for (int i = 0; i < THREADS; ++i)
    {
        params[i].size = work;

        //Check if we're at the last thread
        if (i != (THREADS - 1) && checker == 1)
            params[i].size = remainWork;

        params[i].image = image;
        params[i].w = w;
        params[i].h = h;
        params[i].start = work * i;
        params[i].result = result;

        pthread_create(&tid[i], NULL, threadfn, &params[i]);
        result = params[i].result;
    }

    for (int i = 0; i < THREADS; ++i)
    {
        pthread_join(tid[i], NULL);
    }

    end = clock();
    *elapsedTime += (double)(end - start) / CLOCKS_PER_SEC;

    free(params);

    return result;
}

/*The driver of the program. Check for the correct number of arguments. If wrong print the message: "Usage ./a.out filename"
    Read the image that is passed as an argument at runtime. Apply the filter. Print elapsed time in .3 precision (e.g. 0.006 s). Save the result image in a file called laplacian.ppm. Free allocated memory.
 */
int main(int argc, char *argv[])
{
    double elapsedTime = 0.0;

    if (argc != 2)
    {
        printf("Usage ./a.out filename\n");
        return 0;
    }
    else
    {
        unsigned long int w, h;
        PPMPixel *img;
        PPMPixel *output;

        if (pthread_mutex_init(&lock, NULL) != 0)
        {
            printf("\n mutex init has failed\n");
            return 1;
        }

        img = readImage(argv[1], &w, &h);
        output = apply_filters(img, w, h, &elapsedTime);
        writeImage(output, "laplacian.ppm", w, h);

        printf("Elapsed time: %.3f seconds \n", elapsedTime);

        free(img);
        free(output);
        pthread_mutex_destroy(&lock);

        return 0;
    }
}
