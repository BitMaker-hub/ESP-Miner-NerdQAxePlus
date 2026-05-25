import { Language } from '../models/language.model';
import { createReducer, on } from '@ngrx/store';
import { LanguageActions } from '../actions';

export const languageFeatureKey = 'language';

export interface State {
  current: Language;
}

const getInitialLanguage = (): Language => {
  return (localStorage.getItem('language') || 'en') as Language;
};

export const initialState: State = {
  current: getInitialLanguage()
};

export const reducer = createReducer(
  initialState,
  on(LanguageActions.set, (state, { language }) => ({
    ...state,
    current: language
  }))
);

export const getLanguage = (state: State) => state.current;
