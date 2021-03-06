﻿// cpp-tsp3.cpp: 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include <vector>
#include <random>
#include <iostream>
#include <string>
#include <fstream>
#include <algorithm>
#include <sstream>

using namespace std;

//#define DEBUG

/**
 * \brief 随机数生成器
 */
random_device rd;
default_random_engine eng(rd());
uniform_real_distribution<double> urd(0, 1);

struct point
{
	string name;
	double x;
	double y;
};

// 染色体
class chromosome
{
	/**
	 * \brief 序列组合，存储点本身在 GA::points_ 里面的索引
	 */
	vector<int> sequence_;
	/**
	 * \brief 表示点之间的距离，从外部传入，无法修改
	 */
	const vector<vector<double>>& dist_;

	/**
	 * \brief 从 sequence_ 中随机选取一个点，（除了第一个点以外）。返回该点的下标
	 * \return 
	 */
	int select_point() const
	{
		const int n = sequence_.size();
		return rd() % (n - 1) + 1;
	}

	/**
	 * \brief 路径长度
	 */
	double sum_length_;

	void calc_sum_length()
	{
		// 根据序列获取路径长度。由于路径是一个环路，还要加上从最后一个点到第一个点的长度
		double sum = dist_[sequence_[0]][sequence_[sequence_.size() - 1]];
		for (int i = 1; i < sequence_.size(); ++i)
		{
			const int idx1 = sequence_[i - 1];
			const int idx2 = sequence_[i];
			sum += dist_[idx1][idx2];
		}
		sum_length_ = sum;
	}

public:

	/**
	 * \brief 生成一个长度为dist.size()的染色体，染色体内的编号随机
	 * \param dist 距离矩阵
	 */
	chromosome(const vector<vector<double>>& dist)
		: dist_(dist)
	{
		const int n = dist.size();
		sequence_.reserve(n);
		for (int i = 0; i < n; i++)
		{
			sequence_.push_back(i);
		}
		// 随机选择来进行交换，但第一个元素永远是0。（第一个点永远是起点）
		for (int i = 1; i < n; i++)
		{
			swap(sequence_[i], sequence_[select_point()]);
		}

		///使用改良圈法来取得较优的解
		int flag = 1;
		while (flag)
		{
			flag = 0;
			//不断选取uv子串，尝试倒置uv子串的顺序后解是否更优，如果更优则变更
			for (int u = 1; u < n - 2; u++)
			{
				for (int v = u + 1; v < n - 1; v++)
				{
					if (dist_[sequence_[u]][sequence_[v + 1]] + dist_[sequence_[u - 1]][sequence_[v]] <
						dist_[sequence_[u]][sequence_[u - 1]] + dist_[sequence_[v]][sequence_[v + 1]])
					{
						for (int k = u; k <= (u + v) / 2; k++)
						{
							swap(sequence_[k], sequence_[v - (k - u)]);
							flag = 1;
						}
					}
				}
			}
		}

		calc_sum_length();
	}

	chromosome(const chromosome& other)
		: sequence_(other.sequence_),
		  dist_(other.dist_),
		  sum_length_(other.sum_length_)
	{
	}

	chromosome(chromosome&& other) noexcept
		: sequence_(std::move(other.sequence_)),
		  dist_(other.dist_),
		  sum_length_(other.sum_length_)
	{
	}

	chromosome& operator=(const chromosome& other)
	{
		if (this == &other)
			return *this;
		sequence_ = other.sequence_;
		sum_length_ = other.sum_length_;
		return *this;
	}

	bool operator<(const chromosome& other) const
	{
		return get_length() < other.get_length();
	}

	/**
	 * \brief 获取路径长度
	 * \return 
	 */
	double get_length() const
	{
		return sum_length_;
	}

	/**
	 * \brief 突变：取3个点u<v<w,把子串[u,v]插到w后面
	 */
	void mutation()
	{
		const int n = sequence_.size();

		int u, v, w;
		u = select_point();
		do
		{
			v = select_point();
		}
		while (u == v);
		do
		{
			w = select_point();
		}
		while (w == u || w == v);
		if (u > v)
			swap(u, v);
		if (v > w)
			swap(v, w);
		if (u > v)
			swap(u, v);

		vector<int> res;
		res.reserve(n);
		const back_insert_iterator<vector<int>> bi(res);

		const auto begin_iter = sequence_.cbegin();
		copy(begin_iter, begin_iter + u, bi);
		copy(begin_iter + v, begin_iter + w, bi);
		copy(begin_iter + u, begin_iter + v, bi);
		copy(begin_iter + w, sequence_.cend(), bi);

		sequence_ = std::move(res);
		calc_sum_length();
	}

	/**
	 * \brief 对两条染色体进行交叉
	 * \param that 另一条染色体
	 */
	void crossover(chromosome& that)
	{
		const int n = sequence_.size();
		const int sentinel = select_point(); //基因划分分界点

		for (int j = sentinel; j < n; j++)
		{
			//交换随机选的2个个体的基因后半段，也就是交配
			swap(sequence_[j], that.sequence_[j]);
		}
		//交换后可能发生冲突，需要解除冲突
		//保留F前面的部分不变，F后面的部分有冲突则交换
		vector<int> num1(n), num2(n);
		for (int j = 0; j < n; j++)
		{
			num1[sequence_[j]]++;
			num2[that.sequence_[j]]++;
		}
		vector<int> v1;
		vector<int> v2;
		v1.reserve(n);
		v2.reserve(n);
		for (int j = 0; j < n; j++)
		{
			if (num1[sequence_[j]] == 2)
			{
				v1.push_back(sequence_[j]);
			}
		}
		for (int j = 0; j < n; j++)
		{
			if (num2[that.sequence_[j]] == 2)
			{
				v2.push_back(that.sequence_[j]);
			}
		}
		int p1 = 0, p2 = 0;
		for (int j = sentinel; j < n; j++)
		{
			if (num1[sequence_[j]] == 2)
			{
				sequence_[j] = v2[p2++];
			}
			if (num2[that.sequence_[j]] == 2)
			{
				that.sequence_[j] = v1[p1++];
			}
		}

		calc_sum_length();
		that.calc_sum_length();
	}

	/**
	 * \brief 获取该序列的引用，用于遍历序列
	 * \return 
	 */
	vector<int>& get_point_indexs()
	{
		return sequence_;
	}
};

/**
 * \brief 种群
 */
class population
{
	/**
	 * \brief 种群当中的所有染色体
	 */
	vector<chromosome> chromosomes_;
	/**
	 * \brief 表示点之间的距离，从外部传入，无法修改
	 */
	const vector<vector<double>>& dist_;

	/**
	 * \brief 每个种群中染色体的个数
	 */
	const static int chromosome_num = 100;

	/**
	 * \brief 交叉率
	 */
	constexpr static double crossover_rate = 0.8;

	/**
	 * \brief 变异率
	 */
	constexpr static double mutation_rate = 0.2;

	/**
	 * \brief 最大精英比率（精英留存率）。为 0 时保留一个精英，为负数时不保留精英
	 */
	constexpr static double max_elite_rate = 0.3;

	/**
	 * \brief 对该种群进行交叉操作
	 */
	void do_crossover()
	{
		vector<int> crossovers_idx; // 要交叉的染色体的下标
		for (int i = 0; i < chromosome_num; ++i)
		{
			if (urd(eng) <= crossover_rate)
			{
				// urd(eng) 在 0 到 1 之间， crossover_rate 也是。
				// 因此一个染色体有 crossover_rate 的概率被选中
				crossovers_idx.push_back(i);
			}
		}

		for (int i = 0; i < crossovers_idx.size(); ++i)
		{
			swap(crossovers_idx[i], crossovers_idx[rd() % crossovers_idx.size()]);
		}

		for (int i = 1; i < crossovers_idx.size(); i += 2)
		{
			chromosomes_[crossovers_idx[i - 1]].crossover(chromosomes_[crossovers_idx[i]]);
		}
	}

	/**
	 * \brief 对该种群进行变异操作
	 */
	void do_mutation()
	{
		for (auto& chromosome : chromosomes_)
		{
			if (urd(eng) <= mutation_rate)
			{
				// 原理同上
				chromosome.mutation();
			}
		}
	}

	/**
	 * \brief 使用轮盘赌算法产生下一代种群（不进行交叉和变异）
	 * \return 
	 */
	population generate_by_roulette() const
	{
		double fitness_sum = 0;
		for (const chromosome& chromosome : chromosomes_)
		{
			fitness_sum += 1 / chromosome.get_length();
		}

		double probability_sum = 0;
		vector<double> probabilitys(chromosome_num);
		// 数组： [p0, p0+p1, p0+p1+p2,..., p0+p1+...p(n-1)=1]
		for (int i = 0; i < chromosome_num; ++i)
		{
			probability_sum += (1 / chromosomes_[i].get_length()) / fitness_sum;
			probabilitys[i] = probability_sum;
		}

		vector<chromosome> s1;
		for (int i = 0; i < chromosome_num; ++i)
		{
			double a = urd(eng);
			int j = 0;
			while (j < chromosome_num - 1)
			{
				if (a <= probabilitys[j])
					break;
				++j;
			}
			s1.push_back(chromosomes_[j]);
		}

		return population(std::move(s1), dist_);
	}

	/**
	 * \brief 使用贪心算法来生成下一代种群。这样能保证最快收敛
	 * \return 
	 */
	population generate_by_greedy() const
	{
		population a(*this);
		a.do_crossover();

		population b(*this);
		b.do_mutation();

		vector<chromosome> all;
		all.reserve(chromosomes_.size() * 3);

		all.insert(all.end(), chromosomes_.begin(), chromosomes_.end());
		all.insert(all.end(), a.chromosomes_.begin(), a.chromosomes_.end());
		all.insert(all.end(), b.chromosomes_.begin(), b.chromosomes_.end());

		sort(all.begin(), all.end());

		vector<chromosome> s1(all.begin(), all.begin() + chromosomes_.size());
		return population(std::move(s1), dist_);
	}

	/**
	 * \brief 
	 * 按照比率用本种群中适应度最高的几个染色体替换that种群适应度最低的几个。
	 * 每次放进去的数量都是随机的，但不会超过 max_elite_rate。
	 * 这样做可以加快算法的收敛速度。
	 * \param that 
	 */
	void elite_fix(population& that)
	{
		if (max_elite_rate >= 0)
		{
			sort(chromosomes_.begin(), chromosomes_.end());
			sort(that.chromosomes_.begin(), that.chromosomes_.end());

			if (max_elite_rate == 0)
			{
				that.chromosomes_[chromosome_num - 1] = chromosomes_[0];
			}
			else
			{
				const uniform_real_distribution<double> urd(0, max_elite_rate);

				const int elite_num = urd(eng) * chromosome_num;
				for (int i = 0; i < elite_num; ++i)
				{
					that.chromosomes_[chromosome_num - i - 1] = chromosomes_[i];
				}
			}
		}
	}

public:

	/**
	 * \brief 生成一个有 CHROMOSOME_NUM 个染色体的种群，每个染色体随机生成
	 * \param dist 
	 */
	population(const vector<vector<double>>& dist)
		: dist_(dist)
	{
		chromosomes_.reserve(chromosome_num);
		for (int i = 0; i < chromosome_num; ++i)
		{
			chromosomes_.push_back(chromosome(dist_));
		}
	}

	/**
	 * \brief 从 vector 数组生成 population，数组必须是右值引用
	 * \param chromosomes 
	 * \param dist 
	 */
	population(vector<chromosome>&& chromosomes, const vector<vector<double>>& dist)
		: dist_(dist)
	{
		chromosomes_ = std::move(chromosomes);
	}

	population(const population& other)
		: chromosomes_(other.chromosomes_),
		  dist_(other.dist_)
	{
	}

	population(population&& other) noexcept
		: chromosomes_(std::move(other.chromosomes_)),
		  dist_(other.dist_)
	{
	}

	population& operator=(population&& other) noexcept
	{
		// 确保 dist 一样
		if (this == &other)
			return *this;
		chromosomes_ = std::move(other.chromosomes_);
		return *this;
	}

	/**
	 * \brief 进行一代进化，返回进化之后的新种群
	 * \return 
	 */
	population do_ga()
	{
		/// 使用贪心算法选择染色体
		//return generate_by_greedy();

		/// 使用轮盘赌算法来选择染色体
		population s1(generate_by_roulette());

		//随机选择 s1 中的种群进行交叉
		s1.do_crossover();
		//随机选择s1中的种群进行变异
		s1.do_mutation();

		// 精英主义
		elite_fix(s1);

		return s1;
	}

	/**
	 * \brief 获取最优的染色体
	 * \return 
	 */
	chromosome get_best_chromosome() const
	{
		int idx = 0;
		for (int i = 1; i < chromosome_num; ++i)
		{
			if (chromosomes_[idx].get_length() > chromosomes_[i].get_length())
			{
				idx = i;
			}
		}
		return chromosomes_[idx];
	}
};

/**
 * \brief 表示一次隔离的计算。新建一个对象可以进行和上个对象无关的新计算，可以用于灾变。
 */
class ga
{
	/**
	 * \brief 所有的点
	 */
	const vector<point> &points_;

	/**
	 * \brief 表示点之间的距离。索引是点本身在 points_ 里面的索引
	 */
	const vector<vector<double>> &dist_;

	// 上面的两个成员变量保存了输入数据。把它们变成常量引用以提高效率。
	// 但如果使用指针和 new 实例化 ga 类的话，有可能导致内存问题。
	// 把这两个变量变成普通的常量（去掉&）可以提高代码的稳定性。详情见下方的 main 函数。

	population last_population_;

	/**
	 * \brief 已经进化了多少代
	 */
	int generation_ = 0;

	/**
	 * \brief 如果连续 max_collapse_times 次length差值小于这个，则停止
	 */
	constexpr static double max_length_diff = 1e-15;

	const static int max_collapse_times = 500;

	ostream& data_out_stream_;

public:

	explicit ga(const vector<point>& points, ostream& data_out_stream,
	            const vector<vector<double>>& dist)
		: points_(points), dist_(dist),
		  last_population_(dist_), data_out_stream_(data_out_stream)
	{
	}

	void start_ga()
	{
		print_status();

		double last_length = last_population_.get_best_chromosome().get_length();
		int collapse_time = 0;

		while (true)
		{
			++generation_;
			population new_population(last_population_.do_ga());
			const double new_length = new_population.get_best_chromosome().get_length();
			if (abs(new_length - last_length) < max_length_diff)
			{
				if (++collapse_time >= max_collapse_times)
				{
					break;
				}
			}
			else
			{
				collapse_time = 0;
			}

			last_length = new_length;
			last_population_ = std::move(new_population);
			print_status();
			if (generation_ % 10000 == 0)
			{
				auto best_chromosome = last_population_.get_best_chromosome();
				cout << "第 " << generation_ << " 代，长度： " << best_chromosome.get_length() << endl;
			}
		}

		cout << "代数：" << generation_ << " 长度："
			<< last_population_.get_best_chromosome().get_length() << endl;
	}

	/**
	 * \brief 打印当前状态
	 */
	void print_status() const
	{
		auto best_chromosome = last_population_.get_best_chromosome();
#ifdef DEBUG
		cout << "第 " << generation_ << " 代，长度： " << best_chromosome.get_length() << endl;
#endif

		vector<int>& idxs = best_chromosome.get_point_indexs();
		string seq = "";
		seq += points_[idxs[0]].name;
		for (int i = 1; i < idxs.size(); ++i)
		{
			seq += ' ';
			seq += points_[idxs[i]].name;
		}

#ifdef DEBUG
		cout << seq << endl;
#endif
		data_out_stream_ << best_chromosome.get_length() << ' ' << seq << endl;
	}

	/**
	 * \brief 获取最优结果
	 * \return 
	 */
	double get_min_length() const
	{
		return last_population_.get_best_chromosome().get_length();
	}
};

/**
 * \brief 计算两点距离
 * \param a 
 * \param b 
 * \return 
 */
double get_dist(const point& a, const point& b)
{
	return sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
}

int main()
{
	ifstream fin("in.txt");

	int n;
	fin >> n;
	vector<point> points;
	points.reserve(n);
	for (int i = 0; i < n; ++i)
	{
		point p;
		fin >> p.name >> p.x >> p.y;
		points.push_back(std::move(p));
	}

	/// 计算距离矩阵
	vector<vector<double>> dist(n);
	for (vector<double>& line : dist)
	{
		line.resize(points.size(), 0);
	}
	for (int i = 1; i < points.size(); ++i)
	{
		for (int j = 0; j < i; ++j)
		{
			double dis = get_dist(points[i], points[j]);
			dist[i][j] = dist[j][i] = dis;
		}
	}

	// 连续 max_cast 次灾变得不到更优解时停止迭代
	const int max_cast = 30; // 最大灾变次数
	double min_length = numeric_limits<double>::max();
	string out;

	for (int i = 0; i < max_cast; ++i)
	{
		ostringstream os;

		// 这三个对象在prog中都是引用，在使用纯 RAII 时没有问题。
		// 如果这三个对象使用 RAII 实例化，而 ga 使用 new 在堆上分配内存的话，有可能导致
		// prog 中的引用指向了被释放的内存。详情见 ga 类中的注释。
		ga prog(points, os, dist);
		prog.start_ga();

		double len = prog.get_min_length();
		if (len < min_length)
		{
			min_length = len;
			out = os.str();
			i = 0;
		}
	}

	cout << "最优解：" << min_length << endl;

	ofstream fout("out.txt");
	fout << out;
}
