#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <X11/Xutil.h>
#include <X11/Xlib.h>

#include "struct.h"

#define max(a, b) \
    ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

#define min(a, b) \
    ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _b : _a; })

char WINDOW_NAME[] = "Graphics Window";
char ICON_NAME[] = "Icon";

Display *display;
int screen;
Window main_window;
GC gc;
unsigned long foreground, background;
Colormap screen_colormap;

Bool loop;
Bool triangleInterpolation;
Bool doClean = True;

char lastKeyPress[2] = {'e','e'};

int windowHeight = 400;
int windowWidth = 500;

int Xs[1000];
int Ys[1000];
int currentIndex;

XColor red, black, curColor;
XImage *image;

edge *ET[1000];
edge *AET = NULL;

int xind = -1;
int xx[1000];

int curPixelColor[3];


//////////////// Clipping ///////////////

static int LEFT=1,RIGHT=2,BOTTOM=4,TOP=8; //BitWise codes
static int xl,yl,xh,yh; // windows size (top left and buttom right)


int getcode(int x,int y){
	int code = 0;
	//Perform Bitwise OR to get outcode
	if(y > yh) code |=TOP;
	if(y < yl) code |=BOTTOM;
	if(x < xl) code |=LEFT;
	if(x > xh) code |=RIGHT;
	return code;
}

void performOnLine(int x1, int y1, int x2, int y2)
{
	int outcode1=getcode(x1,y1);
	int outcode2=getcode(x2,y2);
	
	int accept = 0; 	//decides if line is to be draw
	
	while(1)
	{
		float m = (float) (y2 - y1) / (x2 - x1);
		
		//Both points inside. Accept line
		if (outcode1 == 0 && outcode2 == 0)
		{
			accept = 1;
			break;
		}
			//AND of both codes != 0.Line is outside. Reject line
		else if ((outcode1 & outcode2) != 0)
		{
			break;
		} else
		{
			int x, y;
			int temp;
			
			//Decide if point1 is inside, if not, calculate intersection
			if (outcode1 == 0)
				temp = outcode2;
			else
				temp = outcode1;
			
			if (temp & TOP)
			{                //Line clips top edge
				x = x1 + (yh - y1) / m;
				y = yh;
			} else if (temp & BOTTOM)
			{    //Line clips bottom edge
				x = x1 + (yl - y1) / m;
				y = yl;
			} else if (temp & LEFT)
			{        //Line clips left edge
				x = xl;
				y = y1 + m * (xl - x1);
			} else if (temp & RIGHT)
			{    //Line clips right edge
				x = xh;
				y = y1 + m * (xh - x1);
			}
			
			//Check which point we had selected earlier as temp, and replace its co-ordinates
			if (temp == outcode1)
			{
				x1 = x;
				y1 = y;
				outcode1 = getcode(x1, y1);
			} else
			{
				x2 = x;
				y2 = y;
				outcode2 = getcode(x2, y2);
			}
		}
	}
}

///////////////////////////////////////////

void connectX()
{
	display = XOpenDisplay(NULL);
	if (display == NULL)
	{
		fprintf(stderr, "Cannot connect to X\n");
		exit(1);
	}
	screen = DefaultScreen(display);
	foreground = BlackPixel(display, screen);
	background = WhitePixel(display, screen);
}

Window openWindow(int x, int y, int width, int height,
				  int border_width, int argc, char **argv)
{
	Window new_window;
	XSizeHints size_hints;
	new_window = XCreateSimpleWindow(display, DefaultRootWindow(display),
									 x, y, width, height, border_width, foreground,
									 background);
	size_hints.x = x;
	size_hints.y = y;
	size_hints.width = width;
	size_hints.height = height;
	size_hints.flags = PPosition | PSize;
	XSetStandardProperties(display, new_window, WINDOW_NAME, ICON_NAME,
						   None, argv, argc, &size_hints);
	XSelectInput(display, new_window, (ButtonPressMask | KeyPressMask |
									   ExposureMask | PointerMotionMask));
	return (new_window);
}

GC getGC()
{
	GC gc;
	XGCValues gcValues;
	gc = XCreateGC(display, main_window, (unsigned long)0, &gcValues);
	XSetBackground(display, gc, background);
	XSetForeground(display, gc, foreground);
	return (gc);
}

void disconnectX()
{
	XCloseDisplay(display);
	exit(0);
}

void doButtonPressEvent(XButtonEvent *pEvent)
{
	if(lastKeyPress[1]=='d' || lastKeyPress[1]=='t' || lastKeyPress[1]=='e')
	{
		clean();
		lastKeyPress[1] = 'z';
	}
	
	++currentIndex;
	Xs[currentIndex] = pEvent->x;
	Ys[currentIndex] = pEvent->y;
	
	connectPoints(currentIndex);
	
	XFlush(display);
	
	return;
}

void doExposeEvent(XExposeEvent *pEvent)
{
	lastKeyPress[1] = 'e';
	color(currentIndex);
	//printf("in expose last key press: %c %c\n",lastKeyPress[0],lastKeyPress[1]);
}

void doMotionNotifyEvent(XMotionEvent *pEvent)
{
	int x, y;
	char hitLoc[20];
	x = pEvent->x;
	y = pEvent->y;
	sprintf(hitLoc, "Pixel: %d, %d", x, y);
	
	XDrawImageString(display, main_window, gc, 1, windowHeight, hitLoc, strlen(hitLoc));
}

void reset_screen()
{
	XClearWindow(display, main_window);
	XFlush(display);
}

void clean()
{
	for (int i = 0; i <= currentIndex; ++i)
		Xs[i] = Ys[i] = 0;
	currentIndex = -1;
	AET = NULL;
	for (int i = 0; i < windowHeight; ++i)
		ET[i] = NULL;
	XSetForeground(display, gc, black.pixel);
}

void doKeyPressEvent(XKeyEvent *pEvent)
{
	int key_buffer_size = 10;
	char key_buffer[9];
	XComposeStatus compose_status;
	KeySym key_sym;
	XLookupString(pEvent, key_buffer, key_buffer_size, &key_sym, &compose_status);
	if (key_buffer[0] == 'd')
	{
	//	printf("key d last key press: %c %c\n",lastKeyPress[0],lastKeyPress[1]);
		lastKeyPress[0] =lastKeyPress[1];
		lastKeyPress[1] = 'd';
		
		loop = False;
	}
	else if (key_buffer[0] == 'q')
	{
		disconnectX();
	}
	else if (key_buffer[0] == 't')
	{
		lastKeyPress[0] =lastKeyPress[1];
		lastKeyPress[1] = 't';
		
		triangleInterpolation = True;
		loop = False;
	}
	
	else if (key_buffer[0] == 'c')
	{
		lastKeyPress[0] =lastKeyPress[1];
		lastKeyPress[1] = 'c';
		
		reset_screen();
		clean();
	}
	
	else
		printf("You pressed %c\n", key_buffer[0]);
}

void connectPoints(int index)
{
	int i = index;
	int j = max(0, index - 1);
	
	XDrawLine(display, main_window, gc, Xs[j], Ys[j], Xs[i], Ys[i]);
	
	XFlush(display);
}

void add_edge_to_ET(int ymin, int ymax, int xmin, int dx, int dy)
{
	edge *newEdge = (edge *)malloc(sizeof(edge));
	newEdge->ymax = ymax;
	newEdge->x = xmin;
	newEdge->dx = dx;
	newEdge->dy = dy;
	newEdge->next = NULL;
	
	newEdge->sum = 0;
	newEdge->sign = 1;
	if (dx < 0)
		newEdge->sign *= -1;
	if (dy < 0)
		newEdge->sign *= -1;
	
	// If no edges added at specified index
	if (ET[ymin] == NULL)
	{
		ET[ymin] = newEdge;
	}
	else
	{
		//Add to Beginning of List
		if (xmin < ET[ymin]->x)
		{
			edge *nextEdge = ET[ymin];
			newEdge->next = nextEdge;
			ET[ymin] = newEdge;
		}
		else
		{
			// traverse list of edges until proper spot is found
			edge *currEdge = ET[ymin];
			int mCondition = 0;
			while ((currEdge->next != NULL) && (currEdge->next->x <= xmin) && (mCondition == 0))
			{
				if (currEdge->next->x == xmin)
				{
					double m = dx / dy;
					double nextM = currEdge->next->dx / currEdge->next->dy;
					if (nextM <= m)
						currEdge = currEdge->next;
					else
						mCondition = 1;
				}
				else
				{
					currEdge = currEdge->next;
				}
			}
			
			//Add to End of List
			if (currEdge->next == NULL)
			{
				currEdge->next = newEdge;
				//Add to Middle of List
			}
			else
			{
				edge *nextEdge = currEdge->next;
				newEdge->next = nextEdge;
				currEdge->next = newEdge;
			}
		}
	}
}

void build_ET(int n)
{
	int xmin;
	for (int i = 0; i < n; ++i)
	{
		if (Ys[i + 1] > Ys[i])
			xmin = Xs[i];
		else
			xmin = Xs[i + 1];
		
		add_edge_to_ET(min(Ys[i], Ys[i + 1]), max(Ys[i], Ys[i + 1]), xmin, Xs[i + 1] - Xs[i],
					   Ys[i + 1] - Ys[i]);
	}
}

// return index of the first non-empty Edge Table index
int get_minY()
{
	for (int i = 0; i < windowHeight; ++i)
		if (ET[i] != NULL)
			return i;
}

// search ET and AET for any remaining edges, return 1 if at least one is found
Bool edges_to_process()
{
	for (int i = 0; i < windowHeight; ++i)
		if (ET[i] != NULL)
			return 1;
	
	if (AET != NULL)
		return 1;
	
	return 0;
}

// Discard Active Edge List entries where y = ymax
void remove_from_AET(int y)
{
	edge *currEdge = AET;
	edge *prevEdge = NULL;
	while (currEdge != NULL)
	{
		if (currEdge->ymax == y)
		{
			if (prevEdge == NULL)
			{
				//beginning of list, simply remove first element
				currEdge = AET->next;
				AET = currEdge;
			}
			else
			{
				prevEdge->next = currEdge->next;
				currEdge = prevEdge->next;
			}
		}
		else
		{
			// ymax != y, continue down list
			prevEdge = currEdge;
			currEdge = currEdge->next;
		}
	}
}

// Move from ET[y] to AET when ymin = y
void add_to_AET(int y)
{
	// printf("add_to_aet\n");
	if (ET[y] != NULL)
	{
		if (AET == NULL)
		{
			AET = ET[y]; // AET empty
		}
		else
		{
			edge *currEdge = AET;
			//get end of list
			while (currEdge->next != NULL)
			{
				currEdge = currEdge->next;
			}
			//attach edges in ET to end of AET
			currEdge->next = ET[y];
		}
		//remove from ET
		ET[y] = NULL;
	}
}

// Fill pixels on scan line y using pairs of x coordinates from AET
void process_AET(int y)
{
	if (AET == NULL || AET->next == NULL)
		return;
	
	xind = -1;
	
	edge *edge1 = AET;
	
	while (edge1 != NULL)
	{
		int x1 = edge1->x;
		
		xind++;
		xx[xind] = x1;
		
		edge1 = edge1->next;
	}
	
	for (int i = 0; i <= xind; i++)
	{
		for (int j = i + 1; j <= xind; j++)
		{
			if (xx[i] > xx[j])
			{
				int tmp = xx[i];
				xx[i] = xx[j];
				xx[j] = tmp;
			}
		}
	}
	
	if (!triangleInterpolation) {
		curPixelColor[0] = 65535;
		curPixelColor[1] = 0;
		curPixelColor[2] = 0;
	}
	
	for (int i = 1; i <= xind; i += 2)
	{
		if (triangleInterpolation)
		{
			for (int j = xx[i - 1]; j <= xx[i]; ++j)
			{
				choosecolor(j, y);
				setColor();
				XDrawPoint(display, main_window, gc, j, y);
			}
		}
		else {
			XDrawLine(display, main_window, gc, xx[i - 1], y, xx[i], y);
			XFlush(display);
		}
	}
}

// Update all edges in the AET
void updateAET()
{
	edge *currEdge = AET;
	while (currEdge != NULL)
	{
		if (currEdge->dy != 0)
		{
			currEdge->sum += abs(currEdge->dx);
			while (currEdge->sum > abs(currEdge->dy))
			{
				currEdge->sum -= abs(currEdge->dy);
				currEdge->x += currEdge->sign;
			}
			//currEdge->x += (float) currEdge->dx / (float) currEdge->dy;
		}
		currEdge = currEdge->next;
	}
}

void color(int n)
{
	XSetForeground(display, gc, red.pixel);
	
	build_ET(currentIndex);
	
	int y = get_minY();
	
	while (edges_to_process())
	{
		// Discard Active Edge List entries where y = ymax
		remove_from_AET(y);
		
		// Move from Edge Table[y] to Active Edge List when ymin = y
		add_to_AET(y);
		
		// Fill pixels on scan line y using pairs of x coordinates from AET
		process_AET(y);
		
		// Update all edges in the AET
		updateAET();
		
		y++;
	}
	
	return;
}

void setColor()
{
	curColor.pixel = ((curPixelColor[2] & 0xff) | ((curPixelColor[1] & 0xff) << 8) | ((curPixelColor[0] & 0xff) << 16));
	XSetForeground(display, gc, curColor.pixel);
}

double triarea(double dX0, double dY0, double dX1, double dY1, double dX2, double dY2)
{
	double dArea = ((dX1 - dX0)*(dY2 - dY0) - (dX2 - dX0)*(dY1 - dY0)) / 2.0;
	return (dArea > 0.0) ? dArea : -dArea;
}


void choosecolor(int x, int y)
{
	curPixelColor[0] = (int)(255.0 * (triarea(x, y, Xs[1], Ys[1], Xs[2], Ys[2]) / (triarea(Xs[0], Ys[0], Xs[1], Ys[1], Xs[2], Ys[2]))));
	curPixelColor[1] = (int)(255.0 * (triarea(x, y, Xs[0], Ys[0], Xs[2], Ys[2]) / (triarea(Xs[0], Ys[0], Xs[1], Ys[1], Xs[2], Ys[2]))));
	curPixelColor[2] = (int)(255.0 * (triarea(x, y, Xs[0], Ys[0], Xs[1], Ys[1]) / (triarea(Xs[0], Ys[0], Xs[1], Ys[1], Xs[2], Ys[2]))));
}


int main(int argc, char **argv)
{
	connectX();
	
	main_window = openWindow(10, 20, windowWidth, windowHeight, 5, argc, argv);
	
	gc = getGC();
	
	XMapWindow(display, main_window);
	
	XFlush(display);
	
	XEvent event;
	
	
	unsigned int h,w;
	int x,y;
	int g,hh;
	int num = DefaultScreen(display);

//	XGetGeometry(display,RootWindow(display, num),&main_window,&x,&y,&h,&w,&g,&hh);
//	printf("h: %d  w: %d",h,w);
	
	
	/* get colors */
	screen_colormap = DefaultColormap(display, DefaultScreen(display));
	Status st;
	st = XAllocNamedColor(display, screen_colormap, "black", &black, &black);
	st = XAllocNamedColor(display, screen_colormap, "red", &red, &red);


cycle:
	
	loop = True;
	triangleInterpolation = False;
	
	if (doClean) {
		clean();
		doClean = False;
	}
	
	XSetForeground(display, gc, black.pixel);
	//XFlush(display);
	
	while (loop)
	{
		XNextEvent(display, &event);
		switch (event.type)
		{
			case ButtonPress:
				printf("Button Pressed\n");
				doButtonPressEvent(&event);
				break;
			
			case KeyPress:
				printf("Key pressed\n");
				doKeyPressEvent(&event);
				break;
			
			case Expose:
				printf("Expose event\n");
				doExposeEvent(&event);
				break;
			
			case MotionNotify:
				doMotionNotifyEvent(&event);
				break;
		}
	}
	
	printf("Total %d entered points\n", currentIndex + 1);
	/*printf("intput points\n");
	for(int i=0;i<currentIndex;i++)
	printf("%d  %d\n", Ys[i], Xs[i]);*/
	
	// add first point at the end so we have first-last connection
	++currentIndex;
	Xs[currentIndex] = Xs[0];
	Ys[currentIndex] = Ys[0];
	connectPoints(currentIndex);
	
	color(currentIndex);
	
	//printf("in main last key press: %c %c\n",lastKeyPress[0],lastKeyPress[1]);
	
	
	goto cycle;
}