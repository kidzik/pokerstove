#include <fstream>
#include <iostream>
#include <vector>
#include <stdlib.h>
#include <boost/program_options.hpp>
#include <pokerstove/penum/ShowdownEnumerator.h>
#include <pokerstove/peval/Card.h>
#include <pokerstove/peval/OmahaHighHandEvaluator.h>
#include <algorithm>    // std::shuffle
#include <random>       // std::default_random_engine

using namespace pokerstove;
namespace po = boost::program_options;
using namespace std;

template <typename T1, typename T2>
struct more_first {
    typedef pair<T1, T2> type;
    bool operator ()(type const& a, type const& b) const {
        return a.first > b.first;
    }
};

// cards - cards not dealt
// board - board
// hands - vector of hands
// evaluator - object evaluating the showdown
// samples - number of samples
pair<double, double> calculate_equity(std::vector<Card> cards,
			string board,
			vector<string> hands,
			boost::shared_ptr<PokerHandEvaluator> evaluator,
			int samples)
{
  pair<double, double> total(0.0,0.0);

  for (int s = 0; s < samples; s++)
  {
    ShowdownEnumerator showdown;
    std::random_shuffle(cards.begin(), cards.end());

    // construct oppenent's hand and the board by drawing
    // first cards from the shuffled deck
    CardSet newBoard = CardSet(board);
    CardSet opponent = CardSet();

    int toBoard = evaluator->boardSize() - newBoard.size();
    for (int i = 0; i < toBoard; i++){
      newBoard.insert(cards[i]);
    }
    for (int i = toBoard; i < toBoard + evaluator->handSize(); i++){
      opponent.insert(cards[i]);
    }
    // by now everything is dealt

    // prepare an object for evaluation
    vector<CardDistribution> handDists;

    // add hands
    handDists.emplace_back();
    handDists.back().parse(hands[0]); // hero
    handDists.emplace_back();

    // if there are two hands we add them
    // if not, we add the generated opponent
    if (hands.size() > 1)
      handDists.back().parse(hands[1]);
    else
      handDists.back().parse(opponent.str());

    // calcuate the results and store them
    vector<EquityResult> results =
      showdown.calculateEquity(handDists, newBoard, evaluator);

    total.first += results[0].winShares + results[0].tieShares;
    total.second += results[1].winShares + results[1].tieShares;
    // cout << newBoard.str() << results[0].winShares << " " << results[1].winShares <<  endl;
   }
  // mean
  total.first /= samples;
  total.second /= samples;
  // cout << hands[0] << " " << hands[1] << ": " << total.first / samples << endl;
  return total;
}

// for how many rivers we are in the given equity percentile
int river(CardSet handSet,
	  CardSet boardSet,
	  boost::shared_ptr<PokerHandEvaluator> evaluator,
	  int samples){
  CardSet fullSet = CardSet();
  fullSet.fill();
  fullSet ^= handSet;
  fullSet ^= boardSet;
  std::vector<Card> cards = fullSet.cards();
  fullSet |= boardSet;
  int toBoard = evaluator->boardSize() - boardSet.size();

  int count = 0;
  vector<int> distribution;
  distribution.resize(101);
  
  for (int s=0; s < samples; s++){
    std::random_shuffle(cards.begin(), cards.end());

    CardSet newBoard = CardSet(boardSet);

    for (int i = 0; i < toBoard; i++)
      {
	newBoard.insert(cards[i]);
      }
    fullSet ^= newBoard;
    std::vector<Card> riverCards = fullSet.cards();
    fullSet |= newBoard;
    vector<string> hands;
    hands.push_back(handSet.str());
    int bin = (int)(calculate_equity(riverCards, newBoard.str(), hands, evaluator, 100).first * 100);
    distribution[bin]++;
  }

  double total = 0;
  for (int i = 0; i <= 100; i++){
    cout << (double)(distribution[i]) / samples << " ";
    total += distribution[i];
  }
  cout << endl;
  return 0;
}

// for how many rivers we are in the given equity percentile
int vspreflop(CardSet handSet,
	  CardSet boardSet,
	  boost::shared_ptr<PokerHandEvaluator> evaluator,
	      int samples,
	      vector<CardSet> opponents){
  CardSet fullSet = CardSet();
  fullSet.fill();
  fullSet ^= handSet;
  fullSet ^= boardSet;
  double total;
  
  for (int s = 0; s < samples; s++)
    {
      // random from opponents
      CardSet opponentSet = opponents[rand() % opponents.size()];
      if (!fullSet.contains(opponentSet)){
	--s;
	continue;
      }
      fullSet ^= opponentSet;

      vector<string> hands;
      hands.push_back(handSet.str());
      hands.push_back(opponentSet.str());
      total += calculate_equity(fullSet.cards(), boardSet.str(), hands, evaluator, 10).first;
      fullSet |= opponentSet;
    }
  cout << (total / samples) << endl;
  return 0;
}

int main(int argc, char** argv) {
  po::options_description desc("ps-eval, a poker hand evaluator\n");

  desc.add_options()("help,?", "produce help message")
      ("game,g", po::value<string>()->default_value("O"), "game to use for evaluation")
      ("board,b", po::value<string>(), "community cards for he/o/o8")
      ("hand,h", po::value<vector<string>>(), "a hand for evaluation")
      ("top,t", po::value<int>()->default_value(100), "% of top hands")
      ("river,r", po::bool_switch()->default_value(false), "% ")
      ("vspreflop,p", po::bool_switch()->default_value(false), "equity vs top % preflop")
      ("samples,s", po::value<int>()->default_value(10000), "num of monte carlo samples");
  // TODO: Only Omaha works!
  // TODO: Only one hand!
  // TODO: Only one opponent!

  // make hand a positional argument
  po::positional_options_description p;
  p.add("hand", -1);

  po::variables_map vm;
  po::store(po::command_line_parser(argc, argv)
                .style(po::command_line_style::unix_style)
                .options(desc)
                .positional(p)
                .run(),
            vm);
  po::notify(vm);

  // check for help
  if (vm.count("help") || argc == 1) {
    cout << desc << endl;
    return 1;
  }

  // extract the options
  string game = vm["game"].as<string>();
  int top = vm["top"].as<int>();
  string board = (vm.count("board") ) ? vm["board"].as<string>() : "";
  vector<string> hands = vm["hand"].as<vector<string>>();
  int samples = vm["samples"].as<int>();

  // build a deck for simulations
  CardSet fullSet = CardSet();
  fullSet.fill();
  CardSet boardSet = CardSet(board);
  CardSet handSet = CardSet(hands[0]);

  boost::shared_ptr<PokerHandEvaluator> evaluator =
    PokerHandEvaluator::alloc(game);

  if (vm["river"].as<bool>()){
    river(handSet, boardSet, evaluator, samples);
    return 0;
  }

  if (vm["vspreflop"].as<bool>()){
    int nall = 270725;
    int ntop = nall * top / 100;
    vector<CardSet> topHands(ntop);
    ofstream myfile;
    string sortedHands;
    std::ifstream file("Sortedhands.txt");
    std::getline(file, sortedHands);

    string word;
    istringstream iss(sortedHands, istringstream::in);

    int i=0;
    while( iss >> word )     
      {
	topHands[i++] = CardSet(word);
	if (i >= ntop)
	  break;
      }
    // load top hands
    vspreflop(handSet, boardSet, evaluator, samples, topHands);
    return 0;
  }

  // subtract cards already dealt
  fullSet ^= boardSet;

  // list the cards
  std::vector<Card> cards = fullSet.cards();


  // equity vs random hand
  // if (top == 100){
  //   pair<double, double> total = calculate_equity(cards, board, hands, evaluator, samples);
  //   cout << total.first << endl;
  //   return 0;
  // }

  // equity vs pre-flop top 20%
  std::map<CardSet, double> random_hands;

  // get hands
  // CardSet fullSetTmp = CardSet();
  // fullSetTmp.fill();
  // fullSetTmp ^= handSet;
  // fullSetTmp ^= boardSet;
  // std::vector<Card> cards = fullSetTmp.cards();

  for (int s = 0; s < samples; s++){
    std::random_shuffle(cards.begin(), cards.end());
    CardSet opponent = CardSet();

    for (int i = 0; i < evaluator->handSize(); i++){
       opponent.insert(cards[i]);
    }

    fullSet ^= opponent;
    std::vector<Card> cards = fullSet.cards();

    vector<string> opponent_hand;
    opponent_hand.push_back(opponent.str());
    pair<double, double> eq = calculate_equity(cards, board, opponent_hand, evaluator, 100);
    random_hands[opponent] = eq.first;
    fullSet |= opponent;
  }
  
  // sort hands
  std::vector<std::pair<double, CardSet> > eq_random_hands;
  for (std::map<CardSet, double>::iterator it = random_hands.begin(); it != random_hands.end(); it++){
    std::pair<double, CardSet> p(it->second, it->first);
    eq_random_hands.push_back(p);
  }
  sort(eq_random_hands.begin(), eq_random_hands.end(), more_first<double, CardSet>());

  // equity vs top 10%

  fullSet ^= handSet;
  cards = fullSet.cards();

  int nruns = samples; // * top / 100;
  int npos = 0;

  // NOTE: we may have less objects than 'samples' because of duplicates
  for (int s = 0; s < eq_random_hands.size(); s++){
    cout << eq_random_hands[s].second.str() << " " << eq_random_hands[s].first <<  " ";
    CardSet intersection = eq_random_hands[s].second & handSet;
    if (intersection.size()){
      cout << -1 << endl;
      std::pair<string, double> p(eq_random_hands[s].second.str(), -1);
      continue;
    }

    vector<string> vhands;
    vhands.push_back(hands[0]);
    vhands.push_back(eq_random_hands[s].second.str());

    fullSet ^= eq_random_hands[s].second;
    std::vector<Card> cards = fullSet.cards();
    fullSet |= eq_random_hands[s].second;

    pair<double, double> eq = calculate_equity(cards, board, vhands, evaluator, 100);

    cout << eq.second << endl;
  }

  return 0;
}
