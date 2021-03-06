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

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "team 8",
    /* First member's full name */
    "D_cron",
    /* First member's email address */
    "dcron@jungle.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};
// Basic constants and macros
#define WSIZE 4 // 워드 = 헤더 = 풋터 사이즈(bytes)
#define DSIZE 8 // 더블워드 사이즈(bytes)
#define CHUNKSIZE (1<<12) // heap을 이정도 늘린다(bytes)

#define MAX(x, y) ((x) > (y)? (x):(y))

// pack a size and allocated bit into a word 
#define PACK(size, alloc) ((size) | (alloc))

// Read and wirte a word at address p
//p는 (void*)포인터이며, 이것은 직접 역참조할 수 없다.
#define GET(p)     (*(unsigned int *)(p)) //p가 가리키는 놈의 값을 가져온다
#define PUT(p,val) (*(unsigned int *)(p) = (val)) //p가 가리키는 포인터에 val을 넣는다

// Read the size and allocated fields from address p 
#define GET_SIZE(p)  (GET(p) & ~0x7) // ~0x00000111 -> 0x11111000(얘와 and연산하면 size나옴)
#define GET_ALLOC(p) (GET(p) & 0x1)

// Given block ptr bp, compute address of its header and footer
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) //헤더+데이터+풋터 -(헤더+풋터)

// Given block ptr bp, compute address of next and previous blocks
// 현재 bp에서 WSIZE를 빼서 header를 가리키게 하고, header에서 get size를 한다.
// 그럼 현재 블록 크기를 return하고(헤더+데이터+풋터), 그걸 현재 bp에 더하면 next_bp나옴
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))


/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

static void *coalesce(void *bp);
static void *extend_heap(size_t words);
static void *next_fit(size_t asize);
static void place(void *bp, size_t asize);
/* 
 * mm_init - initialize the malloc package.
 */
static char *heap_listp; //prolouge사이를 가리키는 놈
static char *last_bp;
int mm_init(void)
{
    //Create the initial empty heap
    if((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1) 
        return -1;                              // 불러올 수 없으면 -1 return  
    PUT(heap_listp, 0);                         // Alignment padding
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE,1)); // P.H 8(크기)/1(할당됨)
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE,1)); // P.F 8/1
    PUT(heap_listp + (3*WSIZE), PACK(0,1));     // E.H(헤더로만 구성) 0/1
    heap_listp += (2*WSIZE); // 처음에 항상 prolouge 사이를 가리킴
    // Extend the empty heap with a free block of CHUNKSIZE bytes
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL) //word가 몇개인지 확인해서 넣으려고
        return -1;
    last_bp = (char *)heap_listp;//***************************************************처음에 heap_listp 즉 맨 처음 heap 부분을 last_bp에 저장시킴
    return 0;
}
//연결
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));
    // case 1
    if (prev_alloc && next_alloc){
        last_bp = bp; //***************************************************계속 last_bp를 저장시킴
        return bp;
    }
    // case 2
    else if(prev_alloc && !next_alloc){
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size,0));
        PUT(FTRP(bp), PACK(size,0));//header가 바뀌었으니까 size도 바뀐다!
    }
    // case 3
    else if(!prev_alloc && next_alloc){
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size,0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
        bp = PREV_BLKP(bp); //bp를 prev로 옮겨줌
    }
    // case 4
    else{
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + 
                GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size,0));
        bp = PREV_BLKP(bp); //bp를 prev로 옮겨줌
    }
    last_bp = bp;//***************************************************coalesce 후 bp를 반환하는데, 그걸 last_bp에 저장시킴 
    return bp;
}
// heap을 CHUNKSIZE byte로 확장하고 초기 가용 블록을 생성한다.
// 여기까지 진행되면 할당기는 초기화를 완료하고, application으로부터
// 할당과 반환 요청을 받을 준비를 완료하였다.
// extend_heap은 두 가지 경우에 호출된다.
// 1. heap이 초기화될 때
// 2. mm_malloc이 적당한 맞춤(fit)을 찾지 못했을 때
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    // Allocate an even number of words to maintain alignment
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1) //size를 불러올 수 없으면
        return NULL;
    
    // Initialize free block header/footer and the epilogue header
    PUT(HDRP(bp), PACK(size,0)); // Free block header
    PUT(FTRP(bp), PACK(size,0)); // Free block footer
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0,1)); // New epilogue header

    // Coalesce(연결후 합침) if the previous block was free
    return coalesce(bp);
}
// next_fit함수, next-fit으로 구현
static void *next_fit(size_t asize){
    char *bp = last_bp; //***************************************************init할 때 이미 last_bp를 설정하기 때문에 bp에 그 값을 넣어줌 

    for(bp = NEXT_BLKP(bp); GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)){ // last_bp의 다음bp 부터 시작, 블록 size가 0보다 크면 계속 진행, bp는 nextbp로 계속 옮겨감
        if(!GET_ALLOC(HDRP(bp)) && GET_SIZE(HDRP(bp)) >= asize){ // free block, asize보다 큰 블록은 
            last_bp = bp;//**************************************************** last_bp에 저장
            return bp; // ***************************************************** 그 bp를 return
        }
    }
    //*****************************************************return 에 걸리지 않았다는 것은, 마지막 last_bp부터 돌렸을 때, 
    //*************************************************끝까지 갔는데 fit을 찾을 수 없다는 뜻이고, 그럼 맨 처음부터 찾아야한다.(중간에 free된 블럭에 fit될수있음)
    bp = heap_listp;//**********************************맨 첨부터 돌릴꺼다.
    while(bp < last_bp){//**********************************last_bp전까지만 돌린다.
        bp = NEXT_BLKP(bp);
        if(!GET_ALLOC(HDRP(bp)) && GET_SIZE(HDRP(bp)) >= asize){
            last_bp = bp;
            return bp;
        }
    }//**********************************여기에 걸리지 않았다면,
    return NULL; // No fit
}
//place 함수
static void place(void *bp, size_t asize){
    size_t csize = GET_SIZE(HDRP(bp));

    if ((csize - asize) >= (2*DSIZE)){
        PUT(HDRP(bp), PACK(asize,1));//현재 크기를 헤더에 집어넣고
        PUT(FTRP(bp), PACK(asize,1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize-asize,0));
        PUT(FTRP(bp), PACK(csize-asize,0));
    }
    else{
        PUT(HDRP(bp), PACK(csize,1));
        PUT(FTRP(bp), PACK(csize,1));
    }
}
/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize; //할당할 블록 사이즈
    size_t extendsize;
    char *bp;

    // Ignore spurious requests - size가 0이면 할당x
    if(size <= 0) // == 대신 <=
        return NULL;
    
    // Adjust block size to include overhead and alignment reqs.
    if(size <= DSIZE) // size가 8byte보다 작다면,
        asize = 2*DSIZE; // 최소블록조건인 16byte로 맞춤
    else              // size가 8byte보다 크다면
        asize = DSIZE * ((size+(DSIZE)+(DSIZE-1)) / DSIZE);

    //Search the free list for a fit - 적절한 가용(free)블록을 가용리스트에서 검색
    if((bp = next_fit(asize))!=NULL){
        place(bp,asize); //가능하면 초과부분 분할
        last_bp = bp; //********************************** 만약 검색된 놈이 있으면 last_bp에 넣음
        return bp;
    }

    //No fit found -> Get more memory and place the block
    extendsize = MAX(asize,CHUNKSIZE);
    if((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    place(bp,asize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size,0));
    PUT(FTRP(bp), PACK(size,0));
    coalesce(bp);    
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    copySize = GET_SIZE(HDRP(oldptr));//copySize는 우리가 바꾸고 싶은 블록의 size이다.
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    //메모리의 특정한 부분으로부터 얼마까지의 부분을 다른 메모리 영역으로
    // 복사해주는 함수(old_ptr로부터 copySize만큼의 문자를 newptr로 복사해라)
    mm_free(oldptr);
    return newptr;
}














