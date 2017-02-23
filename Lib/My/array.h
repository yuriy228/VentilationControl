/***********************************************************************************
*	File		: efcArray.h
*	Description : Header for a MFC efcArray-like class.
***********************************************************************************/

/* CPPDOC_BEGIN_EXCLUDE  */
#ifndef __EFC_ARRAY_H__
#define __EFC_ARRAY_H__

//#include <new>

#define EFC_INLINE inline
#define BYTE uint8
#define EFC_ASSERT(x)
#define EFC_ASSERT_VALID(x)

/*
#ifndef __PLACEMENT_NEW_INLINE
#define __PLACEMENT_NEW_INLINE
inline void *__cdecl operator new(size_t, void *_P)
   {return (_P); }
#if     _MSC_VER >= 1200
inline void __cdecl operator delete(void *, void *)
   {return; }
#endif
#endif
*/


template<class TYPE>
EFC_INLINE void efcConstructElements(TYPE* pElements, int nCount)
{
   EFC_ASSERT(nCount > 0);

   // first do bit-wise zero initialization
   memset((void*)pElements, 0, nCount * sizeof(TYPE));

   // then call the constructor(s)
   for (; nCount--; pElements++)
	  ::new((void*)pElements) TYPE;
}

template<class TYPE>
EFC_INLINE void efcDestructElements(TYPE* pElements, int nCount)
{
   // call the destructor(s)
   for (; nCount--; pElements++)
      pElements->~TYPE();
}

template<class TYPE>
EFC_INLINE void efcCopyElements(TYPE* pDest, const TYPE* pSrc, int nCount)
{
   // default is element-copy using assignment
   while (nCount--)
      *pDest++ = *pSrc++;
}
/* CPPDOC_END_EXCLUDE  */


/** The efcArray class supports arrays that are are similar to C arrays, but can dynamically shrink and grow as necessary. 
* <p>This class is an analog of MFC efcArray.</p>
* @param TYPE Template parameter specifying the type of objects stored in the array. TYPE is a parameter that is returned by efcArray.
* @param ARG_TYPE Template parameter specifying the argument type used to access objects stored in the array. Often a reference to TYPE. ARG_TYPE is a parameter that is passed to efcArray.
*<BR><BR><DT><B>Remarks: </B>
*   <p>Array indexes always start at position 0. You can decide whether to fix the upper bound or allow the array to expand when you add elements past the current bound. Memory is allocated contiguously to the upper bound, even if some elements are null.</p>
*   <p>As with a C array, the access time for a efcArray indexed element is constant and is independent of the array size. </p>
*   <p><B>Tip</B> Before using an array, use {@link efcArray#SetSize} to establish its size and allocate memory for it. If you do not use {@link efcArray#SetSize}, adding elements to your array causes it to be frequently reallocated and copied. Frequent reallocation and copying are inefficient and can fragment memory.</p>
*/
template<class TYPE, class ARG_TYPE = const TYPE&>
class efcArray
{
public:
// Construction
   efcArray();
   efcArray( int nSize );
private:
   //efcArray( const efcArray &arr );			// not implemented
   //void operator = ( const efcArray &arr );	// not implemented

public:
// Attributes
   int GetSize() const;
   int GetUpperBound() const;
   void SetSize(int nNewSize, int nGrowBy = -1);

// Operations
   // Clean up
   void FreeExtra();
   void RemoveAll();

   // Accessing elements
   const TYPE& GetAt(int nIndex) const;
   void SetAt(int nIndex, ARG_TYPE newElement);
   TYPE& ElementAt(int nIndex);

   // Direct Access to the element data (may return NULL)
   const TYPE* GetData() const;
   TYPE* GetData();

   // Potentially growing the array
   void SetAtGrow(int nIndex, ARG_TYPE newElement);
   int Add(ARG_TYPE newElement);
   TYPE& AddEmpty();
   int Append(const efcArray& src);
   void Copy(const efcArray& src);

   // overloaded operator helpers
   const TYPE& operator[](int nIndex) const;
   TYPE& operator[](int nIndex);

   // Operations that move elements around
   void InsertAt(int nIndex, ARG_TYPE newElement, int nCount = 1);
   TYPE& InsertAtEmpty( int nIndex );
   void RemoveAt(int nIndex, int nCount = 1);
   void InsertAt(int nStartIndex, efcArray* pNewArray);

// Implementation
protected:
/* CPPDOC_BEGIN_EXCLUDE  */
   TYPE* m_pData;   // the actual array of data
   int m_nSize;     // # of elements (upperBound - 1)
   int m_nMaxSize;  // max allocated
   int m_nGrowBy;   // grow amount
/* CPPDOC_END_EXCLUDE  */
public:
   ~efcArray();
};

/////////////////////////////////////////////////////////////////////////////
// efcArray<TYPE, ARG_TYPE> inline functions

/** Returns the size of the array. Since indexes are zero-based, the size is 1 greater than the largest index.*/
template<class TYPE, class ARG_TYPE>
EFC_INLINE int efcArray<TYPE, ARG_TYPE>::GetSize() const
   { return m_nSize; }

/** Returns the current upper bound of this array. Because array indexes are zero-based, this function returns a value 1 less than GetSize. 
* <p>The condition GetUpperBound( ) = –1 indicates that the array contains no elements.</p>*/
template<class TYPE, class ARG_TYPE>
EFC_INLINE int efcArray<TYPE, ARG_TYPE>::GetUpperBound() const
   { return m_nSize-1; }

/** Removes all the elements from this array. If the array is already empty, the function still works.*/
template<class TYPE, class ARG_TYPE>
EFC_INLINE void efcArray<TYPE, ARG_TYPE>::RemoveAll()
   { SetSize(0, -1); }

/** Returns the array element at the specified index.
* @param TYPE Template parameter specifying the type of the array elements.
* @param nIndex An integer index that is greater than or equal to 0 and less than or equal to the value returned by {@link efcArray#GetUpperBound}.
* @return The array element currently at this index.
*<BR><BR><DT><B>Remarks: </B>
*   <p>Passing a negative value or a value greater than the value returned by {@link efcArray#GetUpperBound} will result in a failed assertion.</p>
*/
template<class TYPE, class ARG_TYPE>
EFC_INLINE const TYPE& efcArray<TYPE, ARG_TYPE>::GetAt(int nIndex) const
   { EFC_ASSERT(nIndex >= 0 && nIndex < m_nSize);
      return m_pData[nIndex]; }

/** Sets the array element at the specified index. SetAt will not cause the array to grow. Use {@link efcArray#SetAtGrow} if you want the array to grow automatically.
* @param nIndex An integer index that is greater than or equal to 0 and less than or equal to the value returned by {@link efcArray#GetUpperBound}.
* @param ARG_TYPE Template parameter specifying the type of arguments used for referencing array elements.
* @param newElement The new element value to be stored at the specified position.
* @return The array element currently at this index.
*<BR><BR><DT><B>Remarks: </B>
*   <p>You must ensure that your index value represents a valid position in the array. If it is out of bounds, then the Debug version of the library asserts.</p>
*/
template<class TYPE, class ARG_TYPE>
EFC_INLINE void efcArray<TYPE, ARG_TYPE>::SetAt(int nIndex, ARG_TYPE newElement)
   { EFC_ASSERT(nIndex >= 0 && nIndex < m_nSize);
      m_pData[nIndex] = newElement; }

/** Returns a temporary reference to the specified element within the array. It is used to implement the left-side assignment operator for arrays.
* @param TYPE Template parameter specifying the type of elements in the array.
* @param nIndex An integer index that is greater than or equal to 0 and less than or equal to the value returned by {@link efcArray#GetUpperBound}.
* @return A reference to an array element.
*<BR><BR><DT><B>Remarks: </B>
*   <p>Passing a negative value or a value greater than the value returned by {@link efcArray#GetUpperBound} will result in a failed assertion.</p>
*/
template<class TYPE, class ARG_TYPE>
EFC_INLINE TYPE& efcArray<TYPE, ARG_TYPE>::ElementAt(int nIndex)
   { EFC_ASSERT(nIndex >= 0 && nIndex < m_nSize);
      return m_pData[nIndex]; }

/** Use this member function to gain direct access to the elements in an array. If no elements are available, GetData returns a null value. 
* @param TYPE Template parameter specifying the type of elements in the array.
* @return A pointer to an array element.
*<BR><BR><DT><B>Remarks: </B>
*   <p>While direct access to the elements of an array can help you work more quickly, use caution when calling GetData; any errors you make directly affect the elements of your array.</p>
*/
template<class TYPE, class ARG_TYPE>
EFC_INLINE const TYPE* efcArray<TYPE, ARG_TYPE>::GetData() const
   { return (const TYPE*)m_pData; }

/** Use this member function to gain direct access to the elements in an array. If no elements are available, GetData returns a null value. 
* @param TYPE Template parameter specifying the type of elements in the array.
* @return A pointer to an array element.
*<BR><BR><DT><B>Remarks: </B>
*   <p>While direct access to the elements of an array can help you work more quickly, use caution when calling GetData; any errors you make directly affect the elements of your array.</p>
*/
template<class TYPE, class ARG_TYPE>
EFC_INLINE TYPE* efcArray<TYPE, ARG_TYPE>::GetData()
   { return (TYPE*)m_pData; }

/** Adds a new element to the end of an array, growing the array by 1. If {@link efcArray#SetSize} has been used with an nGrowBy value greater than 1, then extra memory may be allocated. However, the upper bound will increase by only 1.
* @param ARG_TYPE Template parameter specifying the type of arguments referencing elements in this array.
* @param newElement The element to be added to this array.
* @return The index of the added element.
*<BR><BR><DT><B>Remarks: </B>
*   <p>While direct access to the elements of an array can help you work more quickly, use caution when calling GetData; any errors you make directly affect the elements of your array.</p>
*/
template<class TYPE, class ARG_TYPE>
EFC_INLINE int efcArray<TYPE, ARG_TYPE>::Add(ARG_TYPE newElement)
   { int nIndex = m_nSize;
      SetAtGrow(nIndex, newElement);
      return nIndex; }

template<class TYPE, class ARG_TYPE>
EFC_INLINE TYPE& efcArray<TYPE, ARG_TYPE>::AddEmpty()
   {	SetSize( GetSize() + 1 );
		return ElementAt( GetSize() - 1 ); }


/** These subscript operators are a convenient substitute for the {@link efcArray#SetAt} and {@link efcArray#GetAt} functions. <BR>
* The first operator, called for arrays that are not const, may be used on either the right (r-value) or the left (l-value) of an assignment statement. The second, called for const arrays, may be used only on the right. 
* @param TYPE Template parameter specifying the type of elements in this array.
* @param nIndex Index of the element to be accessed.
*<BR><BR><DT><B>Remarks: </B>
*   <p>The Debug version of the library asserts if the subscript (either on the left or right side of an assignment statement) is out of bounds.</p>
*/
template<class TYPE, class ARG_TYPE>
EFC_INLINE const TYPE& efcArray<TYPE, ARG_TYPE>::operator[](int nIndex) const
   { return GetAt(nIndex); }

/** These subscript operators are a convenient substitute for the {@link efcArray#SetAt} and {@link efcArray#GetAt} functions. <BR>
* The first operator, called for arrays that are not const, may be used on either the right (r-value) or the left (l-value) of an assignment statement. The second, called for const arrays, may be used only on the right. 
* @param TYPE Template parameter specifying the type of elements in this array.
* @param nIndex Index of the element to be accessed.
*<BR><BR><DT><B>Remarks: </B>
*   <p>The Debug version of the library asserts if the subscript (either on the left or right side of an assignment statement) is out of bounds.</p>
*/
template<class TYPE, class ARG_TYPE>
EFC_INLINE TYPE& efcArray<TYPE, ARG_TYPE>::operator[](int nIndex)
   { return ElementAt(nIndex); }

/////////////////////////////////////////////////////////////////////////////
// efcArray<TYPE, ARG_TYPE> out-of-line functions

/**Constructs an empty array. The array grows one element at a time.
* <DT><B>Example: </B>
*   <p>// declares an array of points<BR>
*      efcArray<efcPoint,efcPoint> ptArray;</p>
*/
template<class TYPE, class ARG_TYPE>
efcArray<TYPE, ARG_TYPE>::efcArray()
{
   m_pData = NULL;
   m_nSize = m_nMaxSize = m_nGrowBy = 0;
}

template<class TYPE, class ARG_TYPE>
efcArray<TYPE, ARG_TYPE>::efcArray( int nSize )		: efcArray()
{
	SetSize( nSize );
}



/**Destroy array and all it`s content.*/
template<class TYPE, class ARG_TYPE>
efcArray<TYPE, ARG_TYPE>::~efcArray()
{
   EFC_ASSERT_VALID(this);

   if (m_pData != NULL)
   {
	   if ( m_nSize > 0 )
		   efcDestructElements<TYPE>(m_pData, m_nSize);
      delete[] (BYTE*)m_pData;
   }
}

/**Establishes the size of an empty or existing array; allocates memory if necessary. <BR>
* If the new size is smaller than the old size, then the array is truncated and all unused memory is released. 
* @param nNewSize The new array size (number of elements). Must be greater than or equal to 0.
* @param nGrowBy The minimum number of element slots to allocate if a size increase is necessary.
*<BR><BR><DT><B>Remarks: </B>
*   <p>Use this function to set the size of your array before you begin using the array. If you do not use {@link efcArray#SetSize}, adding elements to your array causes it to be frequently reallocated and copied. Frequent reallocation and copying are inefficient and can fragment memory.</p>
*   <p>The nGrowBy parameter affects internal memory allocation while the array is growing. Its use never affects the array size as reported by {@link efcArray#GetSize} and {@link efcArray#GetUpperBound}. If the default value is used,</p>
*/
template<class TYPE, class ARG_TYPE>
void efcArray<TYPE, ARG_TYPE>::SetSize(int nNewSize, int nGrowBy)
{
   EFC_ASSERT_VALID(this);
   EFC_ASSERT(nNewSize >= 0);

   if (nGrowBy != -1)
      m_nGrowBy = nGrowBy;  // set new size

   if (nNewSize == 0)
   {
      // shrink to nothing
      if (m_pData != NULL)
      {
         efcDestructElements<TYPE>(m_pData, m_nSize);
         delete[] (BYTE*)m_pData;
         m_pData = NULL;
      }
      m_nSize = m_nMaxSize = 0;
   }
   else if (m_pData == NULL)
   {
      // create one with exact size
#ifdef SIZE_T_MAX
      EFC_ASSERT(nNewSize <= SIZE_T_MAX/sizeof(TYPE));    // no overflow
#endif
      m_pData = (TYPE*) new BYTE[nNewSize * sizeof(TYPE)];
      efcConstructElements<TYPE>(m_pData, nNewSize);
      m_nSize = m_nMaxSize = nNewSize;
   }
   else if (nNewSize <= m_nMaxSize)
   {
      // it fits
      if (nNewSize > m_nSize)
      {
         // initialize the new elements
         efcConstructElements<TYPE>(&m_pData[m_nSize], nNewSize-m_nSize);
      }
      else if (m_nSize > nNewSize)
      {
         // destroy the old elements
         efcDestructElements<TYPE>(&m_pData[nNewSize], m_nSize-nNewSize);
      }
      m_nSize = nNewSize;
   }
   else
   {
      // otherwise, grow array
      int nGrowBy = m_nGrowBy;
      if (nGrowBy == 0)
      {
         // heuristically determine growth when nGrowBy == 0
         //  (this avoids heap fragmentation in many situations)
         nGrowBy = m_nSize / 8;
         nGrowBy = (nGrowBy < 4) ? 4 : ((nGrowBy > 1024) ? 1024 : nGrowBy);
      }
      int nNewMax;
      if (nNewSize < m_nMaxSize + nGrowBy)
         nNewMax = m_nMaxSize + nGrowBy;  // granularity
      else
         nNewMax = nNewSize;  // no slush

      EFC_ASSERT(nNewMax >= m_nMaxSize);  // no wrap around
#ifdef SIZE_T_MAX
      EFC_ASSERT(nNewMax <= SIZE_T_MAX/sizeof(TYPE)); // no overflow
#endif
      TYPE* pNewData = (TYPE*) new BYTE[nNewMax * sizeof(TYPE)];

      // copy new data from old
      memcpy(pNewData, m_pData, m_nSize * sizeof(TYPE));

      // construct remaining elements
      EFC_ASSERT(nNewSize > m_nSize);
      efcConstructElements<TYPE>(&pNewData[m_nSize], nNewSize-m_nSize);

      // get rid of old stuff (note: no destructors called)
      delete[] (BYTE*)m_pData;
      m_pData = pNewData;
      m_nSize = nNewSize;
      m_nMaxSize = nNewMax;
   }
}

/**Call this member function to add the contents of one array to the end of another. The arrays must be of the same type.<BR>
* If necessary, Append may allocate extra memory to accommodate the elements appended to the array.
* @param src Source of the elements to be appended to an array.
* @return The index of the first appended element.
*/
template<class TYPE, class ARG_TYPE>
int efcArray<TYPE, ARG_TYPE>::Append(const efcArray& src)
{
   EFC_ASSERT_VALID(this);
   EFC_ASSERT(this != &src);   // cannot append to itself

   int nOldSize = m_nSize;
   SetSize(m_nSize + src.m_nSize);
   efcCopyElements<TYPE>(m_pData + nOldSize, src.m_pData, src.m_nSize);
   return nOldSize;
}

/**Use this member function to copy the elements of one array to another.<BR>
* Call this member function to overwrite the elements of one array with the elements of another array.
* @param src Source of the elements to be copied to an array.
*<BR><BR><DT><B>Remarks: </B>
*   <p>Copy does not free memory; however, if necessary, Copy may allocate extra memory to accommodate the elements copied to the array.</p>
*/
template<class TYPE, class ARG_TYPE>
void efcArray<TYPE, ARG_TYPE>::Copy(const efcArray& src)
{
   EFC_ASSERT_VALID(this);
   EFC_ASSERT(this != &src);   // cannot append to itself

   SetSize(src.m_nSize);
   efcCopyElements<TYPE>(m_pData, src.m_pData, src.m_nSize);
}

/**Frees any extra memory that was allocated while the array was grown. This function has no effect on the size or upper bound of the array.*/
template<class TYPE, class ARG_TYPE>
void efcArray<TYPE, ARG_TYPE>::FreeExtra()
{
   EFC_ASSERT_VALID(this);

   if (m_nSize != m_nMaxSize)
   {
      // shrink to desired size
#ifdef SIZE_T_MAX
      EFC_ASSERT(m_nSize <= SIZE_T_MAX/sizeof(TYPE)); // no overflow
#endif
      TYPE* pNewData = NULL;
      if (m_nSize != 0)
      {
         pNewData = (TYPE*) new BYTE[m_nSize * sizeof(TYPE)];
         // copy new data from old
         memcpy(pNewData, m_pData, m_nSize * sizeof(TYPE));
      }

      // get rid of old stuff (note: no destructors called)
      delete[] (BYTE*)m_pData;
      m_pData = pNewData;
      m_nMaxSize = m_nSize;
   }
}

/**Sets the array element at the specified index. The array grows automatically if necessary (that is, the upper bound is adjusted to accommodate the new element).
* @param nIndex An integer index that is greater than or equal to 0.
* @param ARG_TYPE Template parameter specifying the type of elements in the array.
* @param newElement The element to be added to this array. A NULL value is allowed.
*/
template<class TYPE, class ARG_TYPE>
void efcArray<TYPE, ARG_TYPE>::SetAtGrow(int nIndex, ARG_TYPE newElement)
{
   EFC_ASSERT_VALID(this);
   EFC_ASSERT(nIndex >= 0);

   if (nIndex >= m_nSize)
      SetSize(nIndex+1, -1);
   m_pData[nIndex] = newElement;
}

/**Inserts one element (or multiple copies of an element) at a specified index in an array. In the process, it shifts up (by incrementing the index) the existing element at this index, and it shifts up all the elements above it.
* @param nIndex An integer index that may be greater than the value returned by {@link efcArray#GetUpperBound}.
* @param ARG_TYPE Template parameter specifying the type of elements in this array.
* @param newElement The element to be placed in this array.
* @param nCount The number of times this element should be inserted (defaults to 1).
*<BR><BR><DT><B>Remarks: </B>
*   <p>The SetAt function, in contrast, replaces one specified array element and does not shift any elements.</p>
*/
template<class TYPE, class ARG_TYPE>
void efcArray<TYPE, ARG_TYPE>::InsertAt(int nIndex, ARG_TYPE newElement, int nCount /*=1*/)
{
   EFC_ASSERT_VALID(this);
   EFC_ASSERT(nIndex >= 0);    // will expand to meet need
   EFC_ASSERT(nCount > 0);     // zero or negative size not allowed

   if (nIndex >= m_nSize)
   {
      // adding after the end of the array
      SetSize(nIndex + nCount, -1);   // grow so nIndex is valid
   }
   else
   {
      // inserting in the middle of the array
      int nOldSize = m_nSize;
      SetSize(m_nSize + nCount, -1);  // grow it to new size
      // destroy intial data before copying over it
      efcDestructElements<TYPE>(&m_pData[nOldSize], nCount);
      // shift old data up to fill gap
      memmove(&m_pData[nIndex+nCount], &m_pData[nIndex],
         (nOldSize-nIndex) * sizeof(TYPE));

      // re-init slots we copied from
      efcConstructElements<TYPE>(&m_pData[nIndex], nCount);
   }

   // insert new value in the gap
   EFC_ASSERT(nIndex + nCount <= m_nSize);
   while (nCount--)
      m_pData[nIndex++] = newElement;
}


template<class TYPE, class ARG_TYPE>
TYPE& efcArray<TYPE, ARG_TYPE>::InsertAtEmpty( int nIndex )
{
   const int nCount = 1;

   EFC_ASSERT_VALID(this);
   EFC_ASSERT(nIndex >= 0);    // will expand to meet need

   if (nIndex >= m_nSize)
   {
      // adding after the end of the array
      SetSize(nIndex + nCount, -1);   // grow so nIndex is valid
   }
   else
   {
      // inserting in the middle of the array
      int nOldSize = m_nSize;
      SetSize(m_nSize + nCount, -1);  // grow it to new size
      // destroy intial data before copying over it
      efcDestructElements<TYPE>(&m_pData[nOldSize], nCount);
      // shift old data up to fill gap
      memmove(&m_pData[nIndex+nCount], &m_pData[nIndex],
         (nOldSize-nIndex) * sizeof(TYPE));

      // re-init slots we copied from
      efcConstructElements<TYPE>(&m_pData[nIndex], nCount);
   }

   return ElementAt( nIndex );
}



/**Removes one or more elements starting at a specified index in an array. In the process, it shifts down all the elements above the removed element(s). It decrements the upper bound of the array but does not free memory. 
* @param nIndex An integer index that is greater than or equal to 0 and less than or equal to the value returned by {@link efcArray#GetUpperBound}.
* @param nCount The number of elements to remove.(defaults to 1).
*<BR><BR><DT><B>Remarks: </B>
*   <p>If you try to remove more elements than are contained in the array above the removal point, then the Debug version of the library asserts.</p>
*/
template<class TYPE, class ARG_TYPE>
void efcArray<TYPE, ARG_TYPE>::RemoveAt(int nIndex, int nCount)
{
   EFC_ASSERT_VALID(this);
   EFC_ASSERT(nIndex >= 0);
   EFC_ASSERT(nCount >= 0);
   EFC_ASSERT(nIndex + nCount <= m_nSize);

   // just remove a range
   int nMoveCount = m_nSize - (nIndex + nCount);
   efcDestructElements<TYPE>(&m_pData[nIndex], nCount);
   if (nMoveCount)
      memmove(&m_pData[nIndex], &m_pData[nIndex + nCount],
         nMoveCount * sizeof(TYPE));
   m_nSize -= nCount;
}

/**Inserts all the elements from another efcArray collection, starting at the nStartIndex position. 
* @param nStartIndex An integer index that may be greater than the value returned by {@link efcArray#GetUpperBound}.
* @param pNewArray Another array that contains elements to be added to this array.
*<BR><BR><DT><B>Remarks: </B>
*   <p>The SetAt function, in contrast, replaces one specified array element and does not shift any elements.</p>
*/
template<class TYPE, class ARG_TYPE>
void efcArray<TYPE, ARG_TYPE>::InsertAt(int nStartIndex, efcArray* pNewArray)
{
   EFC_ASSERT_VALID(this);
   EFC_ASSERT(pNewArray != NULL);
   EFC_ASSERT_VALID(pNewArray);
   EFC_ASSERT(nStartIndex >= 0);

   if (pNewArray->GetSize() > 0)
   {
      InsertAt(nStartIndex, pNewArray->GetAt(0), pNewArray->GetSize());
      for (int i = 0; i < pNewArray->GetSize(); i++)
         SetAt(nStartIndex + i, pNewArray->GetAt(i));
   }
}

#endif //__EFC_ARRAY_H__
