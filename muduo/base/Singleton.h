// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_SINGLETON_H
#define MUDUO_BASE_SINGLETON_H

#include <boost/noncopyable.hpp>
#include <assert.h>
#include <stdlib.h> // atexit
#include <pthread.h>

namespace muduo {

namespace detail {
// This doesn't detect inherited member functions!
// http://stackoverflow.com/questions/1966362/sfinae-to-check-for-inherited-member-functions
// SFINAE, 替换失败不是错误，使用模板元编程手法检测：
// T是否是类，且是否存在no_destory函数
template<typename T>
struct has_no_destroy {
#ifdef __GXX_EXPERIMENTAL_CXX0X__
  template <typename C> static char test(decltype(&C::no_destroy));
#else
  template<typename C> static char test(typeof(&C::no_destroy));
#endif
  template<typename C> static int32_t test(...);
  const static bool value = sizeof(test<T>(0)) == 1;
};
}

template<typename T>
class Singleton : boost::noncopyable {
public:
  static T &instance() {
	// pthread_once 保持初始化的多线程安全，这里不适用Double Check方法
	// 典型的Double check 方法如下：
	// if (value_ == NULL) {
	//		{
	//			MutexLockGuard lock(mutex_);
	//			if (value_ == NULL)
	//				value_ = new T();
	//		}
	// }
	// 但是这个典型的不正确，原因是由于C++的内存模型存在data race条件
	// C++中的 value_ = new T() 分为三个阶段：
	// 1. 分配内存
	// 2. 在内存位置上调用构造函数
	// 3. 把内存地址赋值给 value_
	// 由于 C++ 的内存模型，2,3步骤可能顺序改变，这导致返回的 value_ 尚未构造完整
	//
	// 此处使用 pthread_once 代替，data race 由 pthread 解决
	pthread_once(&ponce_, &Singleton::init);
	assert(value_ != NULL);
	return *value_;
  }

private:
  Singleton();
  ~Singleton();

  static void init() {
	value_ = new T();
	// 如果 T 不是类或者 T 中不存在 no_destory 成员函数，则注册 exit (进程退出)
	// 时的回调函数，以清理资源，防止资源泄露
	if (!detail::has_no_destroy<T>::value) {
	  ::atexit(destroy);
	}
  }

  static void destroy() {
	// sizeof 不能用于不完整的类型（含void), 函数类型及位域左值
	typedef char T_must_be_complete_type[sizeof(T) == 0 ? -1 : 1];
	T_must_be_complete_type dummy;	// 此部分 make compiler quiet!
	(void) dummy;

	delete value_;
	value_ = NULL;
  }

private:
  static pthread_once_t ponce_;
  static T *value_;
};

template<typename T>
pthread_once_t Singleton<T>::ponce_ = PTHREAD_ONCE_INIT;

template<typename T>
T *Singleton<T>::value_ = NULL;

}
#endif

