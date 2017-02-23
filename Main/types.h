

//typedef unsigned int uint;
typedef uint16_t uint16;
typedef uint8_t uint8;
typedef int16_t int16;
typedef int8_t int8;

typedef unsigned char uchar;


#define TRUE 1
#define FALSE 0

#define MIN( v1, v2 ) ( ((v1) < (v2))? (v1) : (v2) )
#define MAX( v1, v2 ) ( ((v1) > (v2))? (v1) : (v2) )
#define ABS( v ) ( ((v) < 0 )? (-(v)) : (v) )



// Redeclaration of GPIO handlers
//
#define GPIO_A GPIOA
#define GPIO_B GPIOB
#define GPIO_C GPIOC
#define GPIO_D GPIOD
#define GPIO_E GPIOE
#define GPIO_F GPIOF
#define GPIO_G GPIOG
#define GPIO_H GPIOH
#define GPIO_I GPIOI


