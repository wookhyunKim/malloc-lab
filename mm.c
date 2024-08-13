/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"


//  ====================================================================전방선언  
static void *extend_heap(size_t);
static void *coalesce(void *);
static void place(void *,size_t );
static void * find_fit(size_t);

static char *heap_listp = NULL;  // 힙의 시작 포인터
static char *last_bp;
static void *next_fit(size_t );
//  ====================================================================전방선언  

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

// define & Macro
#define WSIZE  4  // header and footer size (bytes)
#define DSIZE  8  // 더블워드 정책에 따른 한 블록의 크기
#define CHUNKSIZE  (1<<12) // page 크기 4KB

#define MAX(x,y)  ((x)>(y) ? (x):(y))

#define PACK(size,alloc) ((size) | (alloc)) // header 에 포함된 데이터는 할당메모리의 사이즈와 할당/가용여부

#define GET(p)  (*(unsigned int *)(p))
#define PUT(p,val)  (*(unsigned int *)(p) = (val))

#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

#define HDRP(bp) ((char *)(bp) - WSIZE)  // header 포인터
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp))- DSIZE) // footer 포인터  GETSIZE(HDRP) 는 header에서 size부분 24/1 -> 24:크기 1:할당블록

#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE))) // 현재 bp에서 header의 size를 알아내어 더해주면 다음 블록의 bp를 알 수 있음
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE))) // 현재 bp에서 이전 블록의 footer의 size를 알아내어 빼면 이전 블록의 bp를 알 수 있음



/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    // heap 영역 초기화
    /*
    prologue header 와 prologue footer 생성 : 메모리의 시작과 끝이 어디인지를 알 수 있도록 한다. 여기에 담긴 정보는 8 size 1 alloc
    epilogue header 메모리의 끝  size : 0 alloc : 1 임. 나중에 메모리의 끝을 찾을 때 size == 0 && alloc == 1인 것으로 찾을 수 있음.
    */
   if((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1){
    return -1;
   }
   PUT(heap_listp,0);
   PUT(heap_listp + (1*WSIZE),PACK(DSIZE,1));
   PUT(heap_listp + (2*WSIZE),PACK(DSIZE,1));
   PUT(heap_listp + (3*WSIZE),PACK(0,1));
   heap_listp += (2 * WSIZE);


   if (extend_heap(CHUNKSIZE/WSIZE) == NULL){
    return -1;
   }
   last_bp = (char *)heap_listp;
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    // // 적당한 맞춤 fit을 찾지 못했을 때, extend_heap 함수를 호출한다. 
    // int newsize = ALIGN(size + SIZE_T_SIZE);
    // void *p = mem_sbrk(newsize);
    // if (p == (void *)-1)
	// return NULL;
    // else {
    //     *(size_t *)p = size;
    //     return (void *)((char *)p + SIZE_T_SIZE);
    // }


    size_t asize; // malloc을 할 때 header , footer도 각각 4바이트 씩도 추가해주어야 하기 때문에 asize라는 변수를 설정
    size_t extendsize; 
    char *bp;

    // 예외처리
    if (size == 0){
        return NULL;
    }

    /*
    input size가 DSIZE보다 작을 때 , header, footer, size만 메모리할당을 하면 되기 때문에 asize를 16바이트로 하면된다.
    */
    if (size <= DSIZE){
        asize = 2 * DSIZE;
    }else{
        asize = DSIZE * ((size + DSIZE + (DSIZE-1)) / DSIZE);
    }


    if ((bp = next_fit(asize)) != NULL){
        place(bp,asize);
        last_bp = bp;
        return bp;
    }

    extendsize = MAX(asize, CHUNKSIZE);
    if((bp = extend_heap(extendsize/WSIZE)) == NULL){
        return NULL;
    }
    place(bp,asize);
    last_bp = bp;
    return bp;


}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));

    PUT(HDRP(ptr),PACK(size,0));
    PUT(FTRP(ptr),PACK(size,0));
    coalesce(ptr);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    

    // if(!GET_ALLOC(NEXT_BLKP(HDRP(ptr)))){

    //     /*
    //     case 1. 뒤에꺼랑 합쳤는데 input size보다 클 때
        
    //     case 2. 끝일떄 sbrk 힙 늘려
    //     */
    //     newptr = coalesce(ptr);
    //     if (GET_SIZE(HDRP(newptr))>=size){
    //         return newptr;
    //     }
        

    // }else if(GET_SIZE(NEXT_BLKP(ptr)==0)){
    //     mem_sbrk(CHUNKSIZE);
    //     newptr = mm_malloc(size);
    //     if (newptr == NULL)
    //         return NULL;

    //     // 기존 블록 크기 가져오기
    //     copySize = GET_SIZE(HDRP(oldptr)) - DSIZE;
    //     if (size < copySize)
    //         copySize = size;
        
    //     // 새로운 위치로 데이터 복사
    //     memcpy(newptr, oldptr, copySize);

    //     // 이전 블록 해제
    //     mm_free(oldptr);
    //     return newptr;
    // }
    // else{
    newptr = mm_malloc(size);
    if (newptr == NULL)
        return NULL;

    // 기존 블록 크기 가져오기
    copySize = GET_SIZE(HDRP(oldptr)) - DSIZE;
    if (size < copySize)
        copySize = size;
    
    // 새로운 위치로 데이터 복사
    memcpy(newptr, oldptr, copySize);

    // 이전 블록 해제
    mm_free(oldptr);
    return newptr;
    // }
}









static void *extend_heap(size_t words){

    /*
    bp : payload pointer
    
    정렬조건을 만족하기 위해 짝수로 할당한다.
    words 를 2로 나눴을 때 홀수면 words +1 * WSIDE 짝수면 words * WSIDE 로 메모리 크기를 정한다.   words +1 => +1을 해서 패딩해준다.

    */

    char *bp;
    size_t size;

    size = (words % 2) ? (words+1) * WSIZE : words *WSIZE;  
    if ((long)(bp = mem_sbrk(size)) == -1){
        return NULL;
    }

    /*
    bp의 header pointer 위치를 찾아서 size와 alloc 여부를 표시
    bp의 footer pointer 위치를 찾아서 size와 alloc 여부를 표시
    다음 블럭의 헤드에 0과 alloc을 표시 -> epilogue header 임을 알려줌
    */

    PUT(HDRP(bp), PACK(size,0));
    PUT(FTRP(bp), PACK(size,0));
    PUT(HDRP(NEXT_BLKP(bp)),PACK(0,1));


    return coalesce(bp);

}


static void *coalesce(void *bp){
      /*
    메모리를 free했을 때 , 가용 블럭을 연결시켜줘서
    단편화를 방지하기 위한 작업

    prev_alloc : 이전 블록의 할당여부
    next_alloc : 다음 블록의 할당여부
    size : 가용 블록의 크기 
    */
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {     
        // CASE 1 : prev 와 next 가 할당되어있을 때       
        last_bp = bp;
        return bp;
    } else if (prev_alloc && !next_alloc) {
        // CASE 2 : prev 만  할당되어있을 때    
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    } else if (!prev_alloc && next_alloc) { 
        // CASE 3 : next 만 할당되어있을 때   
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    } else {                
        // CASE 4 : prev 와 next 가 할당되어 있지 않을 때                
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    last_bp = bp;
    return bp;
}



static void place(void *bp,size_t asize){
    // 적당한 가용공간을 찾아서 그 자리를 malloc으로 채우는 것
    // 아래의 함수에서 bp를 받아 
    // PUT(HDRP(bp),PACK(asize,1))
    // PUT(FTRP(bp),PACK(asize,1))
    size_t csize = GET_SIZE(HDRP(bp));
    // csize : free 크기 
    // asize : malloc 크기
    // 현재 블록의 크기와 요청된 크기를 비교하여 블록을 분할할지 결정
    if ((csize - asize) >= (2 * DSIZE)) {
        PUT(HDRP(bp), PACK(asize, 1));   // 요청된 크기로 현재 블록의 헤더 설정
        PUT(FTRP(bp), PACK(asize, 1));   // 요청된 크기로 현재 블록의 풋터 설정
        bp = NEXT_BLKP(bp);  // 다음 블록으로 이동
        PUT(HDRP(bp), PACK(csize - asize, 0));  // 남은 블록의 헤더 설정
        PUT(FTRP(bp), PACK(csize - asize, 0));  // 남은 블록의 풋터 설정
    } else {
        // 블록 전체를 할당 (분할 불필요)
        PUT(HDRP(bp), PACK(csize, 1));   // 블록 전체를 할당으로 표시
        PUT(FTRP(bp), PACK(csize, 1));
    }
}


static void * find_fit(size_t asize){
    // asize 만큼의 가용공간이 있는지 찾아보고 있으면 반환하는 것
    char * bp = heap_listp;

    // asize 보다 큰 header size를 찾아  
    // 힙 끝까지 탐색 size == 0인것은 에필로그
    while (GET_SIZE(HDRP(bp)) >  0){
        if (!GET_ALLOC(HDRP(bp))  && GET_SIZE(HDRP(bp)) >= asize){
            return bp;
        }
        bp = NEXT_BLKP(bp);
    }
    return NULL;
}



static void *next_fit(size_t adjusted_size) {
    char *bp = last_bp;

    for (bp = NEXT_BLKP(bp); GET_SIZE(HDRP(bp)) != 0; bp = NEXT_BLKP(bp)) {
        if (!GET_ALLOC(HDRP(bp)) && GET_SIZE(HDRP(bp)) >= adjusted_size) {
            last_bp = bp;
            return bp;
        }
    }

    bp = heap_listp;
    while (bp < last_bp) {
        bp = NEXT_BLKP(bp);
        if (!GET_ALLOC(HDRP(bp)) && GET_SIZE(HDRP(bp)) >= adjusted_size) {
            last_bp = bp;
            return bp;
        }
    }

    return NULL;
}