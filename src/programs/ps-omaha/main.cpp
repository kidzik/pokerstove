#include <iostream>
#include <vector>
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

pair<double, double> calculate_equity(std::vector<Card> cards,
			string board,
			vector<string> hands,
			boost::shared_ptr<PokerHandEvaluator> evaluator,
			int samples)
{
  pair<double, double> total(0.0,0.0);
  for (int s = 0; s < samples; s++){
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

  // evaluation (TODO: copied from ps-eval, could be done better!)
    
  // allocate evaluator and create card distributions
  vector<CardDistribution> handDists;

  handDists.emplace_back();
  handDists.back().parse(hands[0]);
  handDists.emplace_back();
  if (hands.size() > 1)
    handDists.back().parse(hands[1]);
  else
    handDists.back().parse(opponent.str());

  // calcuate the results and store them
  vector<EquityResult> results =
    showdown.calculateEquity(handDists, newBoard, evaluator);

  total.first += results[0].winShares + results[0].tieShares;
  total.second += results[1].winShares + results[1].tieShares;
  }
  total.first /= samples;
  total.second /= samples;
  return total;
}

int main(int argc, char** argv) {
  po::options_description desc("ps-eval, a poker hand evaluator\n");

  desc.add_options()("help,?", "produce help message")
      ("game,g", po::value<string>()->default_value("o"), "game to use for evaluation")
      ("board,b", po::value<string>(), "community cards for he/o/o8")
      ("hand,h", po::value<vector<string>>(), "a hand for evaluation")
      ("top,top", po::value<int>()->default_value(100), "% of top hands")
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
  string board = vm.count("board") ? vm["board"].as<string>() : "";
  vector<string> hands = vm["hand"].as<vector<string>>();

  // build a deck for simulations
  CardSet fullSet = CardSet();
  fullSet.fill();
  CardSet boardSet = CardSet(board);
  CardSet handSet = CardSet(hands[0]);

  // subtract cards already dealt
  fullSet ^= boardSet;
  fullSet ^= handSet;

  // list the cards
  std::vector<Card> cards = fullSet.cards();

  int samples = vm["samples"].as<int>();

  boost::shared_ptr<PokerHandEvaluator> evaluator =
    PokerHandEvaluator::alloc(game);

  // equity vs random hand
  if (top == 100){
    pair<double, double> total = calculate_equity(cards, board, hands, evaluator, samples);
    cout << total.first << endl;
  }

  // equity vs pre-flop top 20%
  std::map<CardSet, double> random_hands;

  // get pre-flop hands
  for (int s = 0; s < samples; s++){
    CardSet fullSetTmp = CardSet();
    fullSetTmp.fill();
    fullSetTmp ^= handSet;
    std::vector<Card> cards = fullSetTmp.cards();
    std::random_shuffle(cards.begin(), cards.end());
    CardSet opponent = CardSet();

    for (int i = 0; i < 4; i++){
       opponent.insert(cards[i]);
    }
    CardSet opponentSet = CardSet(opponent);
    fullSetTmp ^= opponentSet;
    cards = fullSetTmp.cards();

    vector<string> opponent_hand;
    opponent_hand.push_back(opponent.str());
    pair<double, double> eq = calculate_equity(cards, "", opponent_hand, evaluator, 100);
    random_hands[opponent] = eq.first;
  }
  
  // sort hands
  std::vector<std::pair<double, CardSet> > eq_random_hands;
  for (std::map<CardSet, double>::iterator it = random_hands.begin(); it != random_hands.end(); it++){
    std::pair<double, CardSet> p(it->second, it->first);
    eq_random_hands.push_back(p);
  }
  sort(eq_random_hands.begin(), eq_random_hands.end(), more_first<double, CardSet>());

  // equity vs top 10%

  int nruns = samples / 10;
  double eq_vs_top_flop = 0.0;
  for (int s = 0; s < nruns; s++){
    vector<string> vhands;
    vhands.push_back(hands[0]);
    vhands.push_back(eq_random_hands[s].second.str());
    cout << eq_random_hands[s].second.str() << endl;
    pair<double, double> eq = calculate_equity(cards, board, vhands, evaluator, 100);
    eq_vs_top_flop += eq.first;
  }
  eq_vs_top_flop /= nruns;
  cout << eq_vs_top_flop << endl;
}
