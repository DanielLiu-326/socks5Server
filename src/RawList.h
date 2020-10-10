//
// Created by Danny on 2020/8/13.
//

#ifndef MEMORYPOOL_RAWLIST_H
#define MEMORYPOOL_RAWLIST_H
#include<cstddef>

namespace ServerUtil {


    template<class Type>
    class RawList {
    public:
        class iterator {
        public:
            Type *ptr;

            inline iterator &operator++();

            inline iterator &operator--();

            inline Type &operator*();

            inline bool operator==(const iterator &);

            inline bool operator!=(const iterator &);

            inline Type *operator->();

        private:
        };

        RawList();

        inline iterator begin();

        inline iterator end();

        inline void pushFront(Type *);

        inline void popFront();

        inline void popBack();

        inline Type *front();

        inline void pushBack(Type *);

        inline Type *back();

        inline size_t size();

        inline void clear();

        inline void remove(Type *node);

        inline iterator remove(iterator iter);

    private:
        size_t _sizeN;
        Type *head;
        Type *rear;
    };


    template<class Type>
    inline typename RawList<Type>::iterator RawList<Type>::begin() {
        iterator it;
        it.ptr = this->head;
        return it;
    }

    template<class Type>
    inline typename RawList<Type>::iterator RawList<Type>::end() {
        iterator it;
        it.ptr = NULL;
        return it;
    }

    template<class Type>
    inline void RawList<Type>::pushFront(Type *node) {
        ++this->_sizeN;
        node->next = this->head;
        node->last = NULL;
        if (this->head)
            (this->head)->last = node;
        else
            this->rear = node;
        this->head = node;
    }

    template<class Type>
    inline Type *RawList<Type>::front() {
        return this->head;
    }

    template<class Type>
    inline void RawList<Type>::pushBack(Type *node) {
        this->_sizeN++;
        node->next = NULL;
        node->last = this->rear;
        if (this->rear)
            (this->rear)->next = node;
        else {
            this->head = node;
        }
        this->rear = node;

    }

    template<class Type>
    inline Type *RawList<Type>::back() {
        return this->rear;
    }

    template<class Type>
    inline size_t RawList<Type>::size() {
        return this->_sizeN;
    }

    template<class Type>
    inline void RawList<Type>::remove(Type *node) {
        if (head == node) {
            this->popFront();
            return;
        }
        if (rear == node) {
            this->popBack();
            return;
        }
        if (node->last == NULL && node->next == NULL)//node not in list
            return;
        node->last->next = node->next;
        node->next->last = node->last;
        node->last = NULL;
        node->next = NULL;
        --this->_sizeN;
    }

    template<class Type>
    inline typename RawList<Type>::iterator RawList<Type>::remove(RawList::iterator iter) {
        iterator ret;
        ret.ptr = iter->next;
        this->remove(iter.ptr);
        return ret;
    }

    template<class Type>
    inline void RawList<Type>::popFront() {
        if (head == NULL)return;
        auto removed = this->head;
        if (this->head == this->rear) {
            head = NULL;
            rear = NULL;
            --this->_sizeN;
        } else {
            this->head = this->head->next;
            if (this->head) {
                this->head->last = NULL;
            }
            removed->last = NULL;
            removed->next = NULL;
            --this->_sizeN;
        }
    }

    template<class Type>
    inline void RawList<Type>::popBack() {
        if (head == NULL)return;
        auto removed = this->rear;
        if (this->head == this->rear) {
            removed->last = NULL;
            removed->next = NULL;
            head = NULL;
            rear = NULL;
            --this->_sizeN;
            return;
        } else {
            this->rear = this->rear->last;
            this->rear->next = NULL;
            --this->_sizeN;
            removed->last = NULL;
            removed->next = NULL;
        }
    }

    template<class Type>
    inline RawList<Type>::RawList():_sizeN(0), head(NULL), rear(NULL) {

    }

    template<class Type>
    void RawList<Type>::clear() {
        this->_sizeN = 0;
        this->head = NULL;
        this->rear = NULL;
    }

    template<class Type>
    inline typename RawList<Type>::iterator &RawList<Type>::iterator::operator++() {
        this->ptr = this->ptr->next;
        return *this;
    }

    template<class Type>
    inline typename RawList<Type>::iterator &RawList<Type>::iterator::operator--() {
        this->ptr = this->ptr->last;
        return *this;
    }

    template<class Type>
    inline Type &RawList<Type>::iterator::operator*() {
        return *this->ptr;
    }

    template<class Type>
    inline bool RawList<Type>::iterator::operator==(const RawList::iterator &it) {
        return it.ptr == this->ptr;
    }

    template<class Type>
    inline bool RawList<Type>::iterator::operator!=(const RawList::iterator &it) {
        return it.ptr != this->ptr;
    }

    template<class Type>
    inline Type *RawList<Type>::iterator::operator->() {
        return this->ptr;
    }

}
#endif MEMORYPOOL_RAWLIST_H
