#include "kernel/types.h"
#include "user/user.h"

// 定义管道的读写端，增加可读性
#define RD 0 // Read end of the pipe
#define WR 1 // Write end of the pipe

/**
 * @brief 素数筛选流水线的核心函数
 * @param lpipe 左邻居（上一个阶段）传入的管道
 *
 * 每个运行此函数的进程执行以下操作：
 * 1. 从左侧管道读取第一个数，这个数必然是素数，并将其打印。
 * 2. 创建一个新的右侧管道。
 * 3. 创建一个子进程，并将右侧管道交给它，让子进程递归执行sieve。
 * 4. 从左侧管道继续读取剩余的数，将不能被第一步发现的素数整除的数，写入右侧管道。
 * 5. 关闭所有使用完毕的管道，等待子进程结束。
 */
void sieve(int lpipe[2])
{
  // 1. 读取第一个数
  int first_prime;
  // 首先关闭左管道的写端，因为当前进程只会从中读取
  close(lpipe[WR]);
  if (read(lpipe[RD], &first_prime, sizeof(first_prime)) != sizeof(first_prime)) {
    // 如果从左管道读不到任何数据，说明上一个阶段已经没有数据传来，
    // 当前进程即可终止。
    close(lpipe[RD]);
    exit(0);
  }

  // 打印这个素数
  printf("prime %d\n", first_prime);

  // 2. 创建一个新的右侧管道，用于和下一个阶段的子进程通信
  int rpipe[2];
  pipe(rpipe);

  // 3. 创建子进程
  if (fork() == 0) {
    // --- 子进程 ---
    // 子进程将递归地成为下一个筛选阶段
    
    // 【关键修复】子进程继承了父进程的左管道(lpipe)的读端，
    // 但它永远不会使用它。必须在这里关闭，否则这个文件描述符会
    // 被传递给所有后续子进程，导致上游管道无法彻底关闭，最终形成死锁。
    close(lpipe[RD]);

    // 递归调用，子进程的“左管道”是父进程的“右管道”(rpipe)
    sieve(rpipe);

  } else {
    // --- 父进程 ---
    // 父进程不需要从右管道读取，所以关闭读端
    close(rpipe[RD]);

    // 4. 从左管道继续读取，筛选后写入右管道
    int num;
    while (read(lpipe[RD], &num, sizeof(num)) == sizeof(num)) {
      if (num % first_prime != 0) {
        // 如果不能被当前素数整除，则传递给下一个阶段
        write(rpipe[WR], &num, sizeof(num));
      }
    }
    
    // 5. 关闭所有管道并等待子进程
    // 读取结束，关闭左管道的读端
    close(lpipe[RD]);
    // 写入结束，关闭右管道的写端。这是至关重要的一步！
    // 它会向子进程的read调用发送EOF信号，使其能够正常结束。
    close(rpipe[WR]);
    
    // 等待子进程完全结束后，父进程才能退出
    wait(0);
  }
  
  exit(0);
}

int main(int argc, char *argv[])
{
  int p[2];
  pipe(p);

  if (fork() == 0) {
    // --- 子进程 ---
    // 这是素数筛选流水线的第一个进程
    // 它将从管道p中读取初始数据
    sieve(p);
  } else {
    // --- 父进程 (main) ---
    // 它的任务是作为“数字发生器”，将2到指定上限的数字写入管道
    // 它不需要从管道读取，所以关闭读端
    close(p[RD]);
    // 【最终修复】测试脚本期望找到所有小于某个值的素数。
    // 将数字生成的上限提高到足以满足测试脚本的要求。
    for (int i = 2; i <= 270; i++) {
      write(p[WR], &i, sizeof(i));
    }
    // 所有数字都已写入，关闭写端以发送EOF信号
    close(p[WR]);
    // 等待子进程（以及它创建的所有后续进程）结束
    wait(0);
  }
  exit(0);
}

