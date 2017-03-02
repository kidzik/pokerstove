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

double calculate_equity(std::vector<Card> cards,
			string board,
			vector<string> hands,
			boost::shared_ptr<PokerHandEvaluator> evaluator)
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

  // evaluation (TODO: copied from ps-eval, could be done better!)
    
  // allocate evaluator and create card distributions
  vector<CardDistribution> handDists;

  handDists.emplace_back();
  handDists.back().parse(hands[0]);
  handDists.emplace_back();
  handDists.back().parse(opponent.str());

  // calcuate the results and store them
  vector<EquityResult> results =
    showdown.calculateEquity(handDists, newBoard, evaluator);

  return results[0].winShares + results[0].tieShares;
}

int main(int argc, char** argv) {
  po::options_description desc("ps-eval, a poker hand evaluator\n");

  desc.add_options()("help,?", "produce help message")
      ("game,g", po::value<string>()->default_value("o"), "game to use for evaluation")
      ("board,b", po::value<string>(), "community cards for he/o/o8")
      ("hand,h", po::value<vector<string>>(), "a hand for evaluation")
      ("top,top", po::value<int>(), "% of top hands")
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
  double total = 0.0;
  for (int s = 0; s < samples; s++)
    total += calculate_equity(cards, board, hands, evaluator);

  // equity vs pre-flop top 20%
  std::map<CardSet, double> random_hands;
  for (int s = 0; s < samples; s++){
    CardSet fullSetTmp = CardSet();
    std::random_shuffle(cards.begin(), cards.end());
    CardSet opponent = CardSet();

    for (int i = 0; i < 5; i++){
      opponent.insert(cards[i]);
    }
    CardSet opponentSet = CardSet(opponent);
    fullSetTmp ^= opponentSet;
    std::vector<Card> cards = fullSetTmp.cards();

    vector<string> opponent_hand;
    opponent_hand.push_back(opponent.str());
    cout << opponent.str() << endl;
    double eq = calculate_equity(cards, board, opponent_hand, evaluator);
    cout << eq << endl;
    random_hands[opponent] = eq;
    cout << "DONE" << endl;
  }
  calculate_equity(cards, board, hands, evaluator);


  cout << (total / double(samples)) << endl;
}
